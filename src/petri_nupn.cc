/// \file petri_nupn.cc
/// \brief Read the NUPN unit tree from a PNML file with expat (a SAX pass).
///
/// Ported from `fr.lip6.move.gal.nupn.NupnHandler`: a stack of open units, and
/// the text of `<places>`/`<subunits>` accumulated then split on whitespace.
/// Only elements inside a `tool="nupn"` toolspecific block are consulted.

#include "hsc/petri/nupn.hh"

#include <expat.h>

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace hsc::petri {

namespace {

/// The SAX state: the tree under construction, plus what is currently open.
struct reader {
  unit_tree tree;
  std::vector<std::string> open;  ///< stack of open unit ids
  std::string text;               ///< accumulated <places>/<subunits> text
  bool in_nupn = false;
  bool do_places = false;
  bool do_subs = false;

  unit& find(const std::string& id) {
    auto it = tree.units.find(id);
    if (it == tree.units.end()) {
      it = tree.units.emplace(id, unit{id, {}, {}}).first;
    }
    return it->second;
  }
};

const char* attr(const XML_Char** atts, const char* key) {
  for (int i = 0; atts[i] != nullptr; i += 2) {
    if (std::strcmp(atts[i], key) == 0) return atts[i + 1];
  }
  return nullptr;
}

void XMLCALL start(void* data, const XML_Char* name, const XML_Char** atts) {
  auto* r = static_cast<reader*>(data);
  if (std::strcmp(name, "toolspecific") == 0) {
    const char* tool = attr(atts, "tool");
    r->in_nupn = tool != nullptr && std::strcmp(tool, "nupn") == 0;
    return;
  }
  if (!r->in_nupn) return;

  if (std::strcmp(name, "structure") == 0) {
    if (const char* root = attr(atts, "root")) r->tree.root = root;
    const char* safe = attr(atts, "safe");
    r->tree.safe = safe == nullptr || std::strcmp(safe, "false") != 0;
  } else if (std::strcmp(name, "unit") == 0) {
    const char* id = attr(atts, "id");
    r->open.push_back(id ? id : "");
    r->find(r->open.back());
  } else if (std::strcmp(name, "places") == 0) {
    r->do_places = true;
    r->text.clear();
  } else if (std::strcmp(name, "subunits") == 0) {
    r->do_subs = true;
    r->text.clear();
  }
}

void XMLCALL chars(void* data, const XML_Char* s, int len) {
  auto* r = static_cast<reader*>(data);
  if (r->do_places || r->do_subs) r->text.append(s, static_cast<std::size_t>(len));
}

void XMLCALL end(void* data, const XML_Char* name) {
  auto* r = static_cast<reader*>(data);
  if (std::strcmp(name, "toolspecific") == 0) {
    r->in_nupn = false;
    return;
  }
  if (!r->in_nupn) return;

  if (std::strcmp(name, "unit") == 0) {
    if (!r->open.empty()) r->open.pop_back();
  } else if (std::strcmp(name, "places") == 0 ||
             std::strcmp(name, "subunits") == 0) {
    unit& u = r->find(r->open.back());
    std::istringstream in(r->text);
    std::string tok;
    while (in >> tok) {
      if (r->do_subs) u.subunits.push_back(tok);
      else u.places.push_back(tok);
    }
    r->text.clear();
    r->do_places = false;
    r->do_subs = false;
  }
}

}  // namespace

unit_tree read_units(const std::string& path) {
  reader r;
  std::FILE* in = std::fopen(path.c_str(), "r");
  if (!in) return r.tree;

  XML_Parser parser = XML_ParserCreate(nullptr);
  XML_SetUserData(parser, &r);
  XML_SetElementHandler(parser, start, end);
  XML_SetCharacterDataHandler(parser, chars);

  char buf[BUFSIZ];
  for (;;) {
    const std::size_t len = std::fread(buf, 1, sizeof(buf), in);
    const bool done = len < sizeof(buf);
    if (XML_Parse(parser, buf, static_cast<int>(len), done) == XML_STATUS_ERROR) {
      std::fprintf(stderr, "nupn: %s at line %ld\n",
                   XML_ErrorString(XML_GetErrorCode(parser)),
                   XML_GetCurrentLineNumber(parser));
      break;
    }
    if (done) break;
  }
  XML_ParserFree(parser);
  std::fclose(in);
  return r.tree;
}

}  // namespace hsc::petri
