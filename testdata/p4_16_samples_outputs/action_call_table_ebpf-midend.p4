#include <core.p4>
#include <ebpf_model.p4>

struct Headers_t {
}

parser prs(packet_in p, out Headers_t headers) {
    state start {
        transition accept;
    }
}

control pipe(inout Headers_t headers, out bool pass) {
    @name("pipe.Reject") action Reject(@name("rej") bit<8> rej, @name("bar") bit<8> bar) {
        pass = rej == 8w0;
        pass = (bar == 8w0 ? false : rej == 8w0);
    }
    @name("pipe.t") table t_0 {
        actions = {
            Reject();
        }
        default_action = Reject(8w1, 8w0);
    }
    apply {
        t_0.apply();
    }
}

ebpfFilter<Headers_t>(prs(), pipe()) main;
