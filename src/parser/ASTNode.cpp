#include <iostream>
#include <sstream>

#include "ASTNode.h"
#include "Errors.h"
#include "StringSet.h"
#include "Parser.h"
#include "Lexer.h"
#include "Util.h"

ASTLocation::operator std::string() const {
	return std::to_string(line + 1) + ":" + std::to_string(column);
}

ASTLocation::operator bool() const {
	return !(line == 0 && column == 0);
}

std::ostream & operator<<(std::ostream &os, const ASTLocation &location) {
	os << std::string(location);
	return os;
}

ASTNode::ASTNode(Parser &parser_, int sym, const ASTLocation &loc, const char *info) {
	parser = &parser_;
	symbol = sym;
	location = loc;
	text = StringSet::intern(info);
}

ASTNode::ASTNode(Parser &parser_, int sym, const ASTLocation &loc, const std::string *info):
	parser(&parser_), symbol(sym), location(loc), text(info) {}

ASTNode::ASTNode(Parser &parser_, int sym, const std::string *info):
	parser(&parser_), symbol(sym), location(cmmLexer.location), text(info) {}

ASTNode::ASTNode(Parser &parser_, int sym, const char *info) {
	parser = &parser_;
	symbol = sym;
	location = cmmLexer.location;
	text = StringSet::intern(info);
}

ASTNode::ASTNode(Parser &parser_, int sym, const ASTLocation &loc): ASTNode(parser_, sym, loc, "") {}

ASTNode::ASTNode(Parser &parser_, int sym): ASTNode(parser_, sym, "") {}

ASTNode::ASTNode(Parser &parser_, int sym, ASTNode *node, const char *info):
		ASTNode(parser_, sym, node->location, info) {
	adopt(node);
}

ASTNode::ASTNode(Parser &parser_, int sym, ASTNode *node, const std::string *info):
		ASTNode(parser_, sym, node->location, info) {
	adopt(node);
}

ASTNode::~ASTNode() {
	for (ASTNode *child: children)
		delete child;
}

ASTNode * ASTNode::operator[](size_t index) const {
	return at(index);
}

ASTNode * ASTNode::at(size_t index) const {
	return *std::next(children.begin(), index);
}

size_t ASTNode::size() const {
	return children.size();
}

bool ASTNode::empty() const {
	return children.empty();
}

ASTNode * ASTNode::adopt(ASTNode *child, bool do_locate) {
	if (child) {
		if (do_locate)
			locate(child);
		children.push_back(child);
		child->parent = this;
	}

	return this;
}

ASTNode * ASTNode::adopt(std::initializer_list<ASTNode *> to_add) {
	for (ASTNode *child: to_add)
		adopt(child);
	return this;
}

ASTNode * ASTNode::absorb(ASTNode *to_absorb) {
	if (!to_absorb)
		return this;

	locate(to_absorb);

	for (auto iter = children.begin(), end = children.end(); iter != end; ++iter) {
		if (*iter == to_absorb) {
			children.erase(iter);
			break;
		}
	}

	for (ASTNode *child: to_absorb->children)
		adopt(child);
	to_absorb->children.clear();
	text = to_absorb->text;
	delete to_absorb;
	return this;
}

ASTNode * ASTNode::clear() {
	for (ASTNode *child: children)
		delete child;
	children.clear();
	return this;
}

ASTNode * ASTNode::copy() const {
	ASTNode *out = new ASTNode();
	out->symbol = symbol;
	out->location = location;
	out->text = text;
	out->parent = parent;
	for (ASTNode *child: children) {
		ASTNode *copy = child->copy();
		copy->parent = out;
		out->children.push_back(copy);
	}

	return out;
}

ASTNode * ASTNode::locate(const ASTNode *source) {
	if (source)
		location = source->location;
	return this;
}

ASTNode * ASTNode::locate(std::initializer_list<const ASTNode *> sources) {
	for (const ASTNode *source: sources) {
		if (source) {
			location = source->location;
			return this;
		}
	}
	return this;
}

ASTNode * ASTNode::locate(const ASTLocation &loc) {
	location = loc;
	return this;
}

long ASTNode::atoi() const {
	if (text->substr(0, 2) == "0x")
		return Util::parseLong(text->substr(2), 16);
	return Util::parseLong(*text);
}

long ASTNode::atoi(int offset) const {
	return Util::parseLong(text->substr(offset));
}

std::string ASTNode::unquote(bool unescape) const {
	if (text->size() < 2 || text->front() != '"' || text->back() != '"')
		throw GenericError(location, "Not a quoted string: " + *text);
	const std::string out = text->substr(1, text->size() - 2);
	return unescape? Util::unescape(out) : out;
}

char ASTNode::getChar() const {
	if (text->size() < 2 || text->front() != '\'' || text->back() != '\'')
		throw GenericError(location, "Not a quoted character: " + *text);
	const std::string out = text->substr(1, text->size() - 2);

	if (out.front() == '\\') {
		if (out.size() == 1)
			throw std::runtime_error("Expected characters after backslash in escape sequence");
		switch (out[1]) {
			case 'n':  return '\n';
			case 'r':  return '\r';
			case 'a':  return '\a';
			case 't':  return '\t';
			case 'b':  return '\b';
			case 'e':  return '\e';
			case '0':  return '\0';
			case '\\': return '\\';
			case '\'':  return '\'';
			case 'x': {
				if (out.size() <= 3)
					throw std::runtime_error("Hexadecimal escape is too close to end of string");
				const char first = out[2], second = out[3];
				if (!isxdigit(first) || !isxdigit(second))
					throw std::runtime_error(std::string("Invalid hexadecimal escape: \\x") + first + second);
				return char(strtol((std::string(1, first) + second).c_str(), nullptr, 16));
			}
			default:
				throw std::runtime_error("Unrecognized character: \\" + std::string(1, out[1]));
		}
	}

	return out.front();
}

const char * ASTNode::getName() const {
	return parser->getName(symbol);
}

void ASTNode::debug(int indent, bool is_last, bool suppress_line) const {
	if (indent == 0 && !suppress_line)
		std::cerr << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
	for (int i = 0; i < indent; ++i) {
		std::cerr << "\e[2m";
		if (i == indent - 1)
			std::cerr << (is_last? "└── " : "├── ");
		else
			std::cerr << "│   ";
		std::cerr << "\e[0m";
	}

	std::cerr << style() << getName() << "\e[0;2m " << location << "\x1b[0;35m " << *text << " ";
	std::cerr << "\e[0m" << debugExtra() << "\n";
	for (ASTNode *child: children)
		child->debug(indent + 1, child == children.back());
}

std::string ASTNode::debugExtra() const {
	return "";
}

std::string ASTNode::style() const {
	return "\e[1m";
}

std::string ASTNode::extractName() const {
	if ((parser == &wasmParser && symbol == WASMTOK_STRING) || (parser == &cmmParser && symbol == CMMTOK_STRING))
		return text->substr(1, text->size() - 2);
	throw GenericError(location, "extractName() was called on an inappropriate symbol: " +
		std::string(parser->getName(symbol)));
}

const std::string * ASTNode::extracted() const {
	return StringSet::intern(extractName());
}

void ASTNode::destroy(std::initializer_list<ASTNode *> to_destroy) {
	for (ASTNode *node: to_destroy) {
		if (node)
			delete node;
	}
}

ASTNode * ASTNode::front() const {
	return children.front();
}

ASTNode * ASTNode::back() const {
	return children.back();
}

decltype(ASTNode::children)::iterator ASTNode::begin() {
	return children.begin();
}

decltype(ASTNode::children)::iterator ASTNode::end() {
	return children.end();
}

decltype(ASTNode::children)::const_iterator ASTNode::begin() const {
	return children.begin();
}

decltype(ASTNode::children)::const_iterator ASTNode::end() const {
	return children.end();
}

decltype(ASTNode::children)::const_iterator ASTNode::cbegin() const noexcept {
	return children.cbegin();
}

decltype(ASTNode::children)::const_iterator ASTNode::cend() const noexcept {
	return children.cend();
}
