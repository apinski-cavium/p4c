#include "error_message.h"

std::string ErrorMessage::getPrefix() const {
    std::string p;
    if (type == MessageType::Error) {
      return "error: ";
    } else if (type == MessageType::Warning) {
       return "warning: ";
    }
    return "";
}

std::string ErrorMessage::getPostfix() const {
    std::string p = prefix;
    if (type == MessageType::Error) {
        if (!p.empty())
	  return " [--Werror=" + p + "]";
    }
    if (type == MessageType::Warning) {
        if (!p.empty())
	  return " [--Wwarn=" + p + "]";
    }
    return "";
}

std::string ErrorMessage::toString() const {
    std::string result = "";
    std::string mainFragment = "";
    if (!locations.empty() && locations.front().isValid()) {
        result += locations.front().toPositionString() + ": ";
#if 0
        mainFragment = locations.front().toSourceFragment();
#endif
    } else {
      result += "p4c: ";
    }

    result += getPrefix() + message + getPostfix() + "\n";// + mainFragment;

#if 0
    for (unsigned i = 1; i < locations.size(); i++) {
        if (locations[i].isValid())
            result += locations[i].toPositionString() + "\n" + locations[i].toSourceFragment();
    }

    result += suffix;

#endif
    return result;
}

std::string ParserErrorMessage::toString() const {
    unsigned line = 0, column = 0;
    cstring file = location.toSourcePositionData(&line, &column);
    cstring locstr;
    if (file != "")
      locstr = Util::printf_format("%s:%d:%d", file, line, column);

    return std::string(locstr.c_str()) + ": error: " + message + "\n";
#if 0
           + location.toSourceFragment().c_str();
#endif
}
