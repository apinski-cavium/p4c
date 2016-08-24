/*
Copyright 2016 VMware, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "simplifyParsers.h"

namespace P4 {

namespace {
// All the classes in this namespace are invoked on each parser
// independently.

class RemoveUnreachableStates : public Transform {
    ParserCallGraph* transitions;
    std::set<const IR::ParserState*> reachable;

 public:
    explicit RemoveUnreachableStates(ParserCallGraph* transitions) :
            transitions(transitions)
    { CHECK_NULL(transitions); setName("RemoveUnreachableStates"); }

    const IR::Node* preorder(IR::P4Parser* parser) override {
        auto start = parser->getDeclByName(IR::ParserState::start);
        if (start == nullptr) {
            ::error("%1%: parser does not have a `start' state", parser);
        } else {
            transitions->reachable(start->to<IR::ParserState>(), reachable);
            // Remove unreachable states from call-graph
            transitions->restrict(reachable);
            LOG1("Parser " << parser << " has " << transitions->size() << " reachable states");
        }
        return parser;
    }

    const IR::Node* preorder(IR::ParserState* state) override {
        if (state->name == IR::ParserState::start ||
            state->name == IR::ParserState::reject ||
            state->name == IR::ParserState::accept)
            return state;
        auto orig = getOriginal<IR::ParserState>();
        if (reachable.find(orig) == reachable.end()) {
            LOG1("Removing unreachable state " << state);
            return nullptr;
        }
        return state;
    }
};

// This is only invoked on parsers
class CollapseChains : public Transform {
    ParserCallGraph* transitions;
    std::map<const IR::ParserState*, const IR::ParserState*> chain;
    ordered_set<const IR::ParserState*> chainStart;

 public:
    explicit CollapseChains(ParserCallGraph* transitions) : transitions(transitions)
    { CHECK_NULL(transitions); setName("CollapseChains"); }

    const IR::Node* preorder(IR::P4Parser* parser) override {
        std::map<const IR::ParserState*, const IR::ParserState*> pred;

        // Find chains s -> s1 -> s2
        // such that these are all the transitions between these states.
        // Collapse these into a single state.
        for (auto oe : *transitions) {
            auto node = oe.first;
            auto outedges = oe.second;
            if (outedges->size() != 1)
                continue;
            auto next = *outedges->begin();
            if (next->name == IR::ParserState::accept ||
                next->name == IR::ParserState::reject ||
                next->name == IR::ParserState::start)
                continue;
            auto callers = transitions->getCallers(next);
            if (callers->size() != 1)
                continue;
            if (!next->annotations->annotations->empty())
                // we are not sure what to do with the annotations
                continue;
            chain.emplace(node, next);
            pred.emplace(next, node);
        }
        if (chain.empty())
            return parser;

        for (auto e : pred) {
            auto crt = e.first;
            while (pred.find(crt) != pred.end()) {
                auto prev = pred.find(crt)->second;
                if (prev == e.first)  // this could be a cycle, e.g., start->start
                    break;
                crt = prev;
            }
            chainStart.emplace(crt);
        }

        auto states = new IR::IndexedVector<IR::ParserState>();
        for (auto s : *parser->states) {
            if (pred.find(s) != pred.end())
                continue;
            if (chainStart.find(s) != chainStart.end()) {
                // collapse chain
                auto components = new IR::IndexedVector<IR::StatOrDecl>();
                auto crt = s;
                LOG1("Chaining states into " << crt);
                const IR::Expression *select = nullptr;
                while (true) {
                    components->append(*crt->components);
                    select = crt->selectExpression;
                    crt = ::get(chain, crt);
                    if (crt == nullptr)
                        break;
                    LOG1("Adding " << crt << " to chain");
                }
                s = new IR::ParserState(s->srcInfo, s->name, s->annotations,
                                        components, select);
            }
            states->push_back(s);
        }

        parser->states = states;
        prune();
        return parser;
    }
};

// This is invoked on each parser separately
class SimplifyParser : public PassManager {
    ParserCallGraph transitions;
 public:
    explicit SimplifyParser(ReferenceMap* refMap) : transitions("transitions") {
        passes.push_back(new ComputeParserCG(refMap, &transitions));
        passes.push_back(new RemoveUnreachableStates(&transitions));
        passes.push_back(new CollapseChains(&transitions));
        setName("SimplifyParser");
    }
};

}  // namespace

const IR::Node* DoSimplifyParsers::preorder(IR::P4Parser* parser) {
    SimplifyParser simpl(refMap);;
    return parser->apply(simpl);
}

}  // namespace P4
