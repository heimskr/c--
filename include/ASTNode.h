#pragma once

#include <initializer_list>
#include <list>
#include <ostream>
#include <set>
#include <string>
#include <unordered_map>

struct ASTLocation {
	size_t line = 0;
	size_t column = 0;

	ASTLocation() = default;
	explicit ASTLocation(size_t line_, size_t column_): line(line_), column(column_) {}

	explicit operator std::string() const;
	explicit operator bool() const;
} __attribute__((aligned(16)));

std::ostream & operator<<(std::ostream &, const ASTLocation &);

class Parser;

class ASTNode {
	private:
		ASTNode() = default;

	public:
		Parser *parser = nullptr;
		int symbol = 0;
		ASTLocation location;
		const std::string *text = nullptr;
		ASTNode *parent = nullptr;
		std::list<ASTNode *> children;
		std::set<std::string> attributes;
		std::unordered_map<std::string, std::string> map;

		ASTNode(Parser &, int sym, const ASTLocation &loc, const char *info);
		ASTNode(Parser &, int sym, const ASTLocation &loc, const std::string *info);
		ASTNode(Parser &, int sym, const char *info);
		ASTNode(Parser &, int sym, const std::string *info);
		ASTNode(Parser &, int sym, const ASTLocation &loc);
		ASTNode(Parser &, int sym);
		/** Constructs an ASTNode that adopts another node and copies its location. */
		ASTNode(Parser &, int sym, ASTNode *, const char *info = "");
		ASTNode(Parser &, int sym, ASTNode *, const std::string *info);
		ASTNode(const ASTNode &) = delete;
		ASTNode(ASTNode &&) = delete;
		ASTNode & operator=(const ASTNode &) = delete;
		ASTNode & operator=(ASTNode &&) = delete;
		virtual ~ASTNode();

		ASTNode * operator[](size_t) const;
		ASTNode * at(size_t) const;
		size_t size() const;
		bool empty() const;
		ASTNode * adopt(ASTNode *, bool do_locate = false);
		ASTNode * adopt(std::initializer_list<ASTNode *>);
		ASTNode * absorb(ASTNode *);
		ASTNode * clear();
		ASTNode * copy() const;
		/** Copies the location from another node. */
		ASTNode * locate(const ASTNode *);
		/** Copies the location from the first node in the list that isn't null. */
		ASTNode * locate(std::initializer_list<const ASTNode *>);
		ASTNode * locate(const ASTLocation &);
		int64_t atoi() const;
		int64_t atoi(int offset) const;
		std::string unquote(bool unescape = true) const;
		char getChar() const;
		const char * getName() const;
		void debug(int indent = 0, bool is_last = false, bool suppress_line = false) const;
		virtual std::string debugExtra() const;
		virtual std::string style() const;
		std::string extractName() const;
		const std::string * extracted() const;

		static void destroy(std::initializer_list<ASTNode *>);

		ASTNode * front() const;
		ASTNode * back() const;
		decltype(children)::iterator begin();
		decltype(children)::iterator end();
		decltype(children)::const_iterator begin() const;
		decltype(children)::const_iterator end() const;
		decltype(children)::const_iterator cbegin() const noexcept;
		decltype(children)::const_iterator cend() const noexcept;
};
