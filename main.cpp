#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <exception>
#include <regex>


enum LexemeType {
    NUMBER,
    KEYWORD,
    COMMENT,
    STR_CHAR,
    OPERATOR,
    DELIMITER,
    IDENTIFIER,
    DIRECTIVE,
};


// Attention!
// since std::regex doesn't support lookbehind
// ONLY FIRST GROUP is considered, not entire match
std::regex REG_NUMBER {
    R"((?:^|\W)()"
    // floats
    R"([+-]?\d+\.\d+([eE][-+]?\d+)?)"
    R"(|[-+]?(?:[1-9]\d*|0)[eE][-+]?\d+)"
    // hexadecimal
    R"(|[+-]?&H[0-9a-fA-F]+)"
    R"(|[+-]?&H[0-9a-fA-F]+)"
    // decimals
    R"(|[-+]?(?:[1-9]\d*|0)(?![[:digit:]]))"
    R"(|[-+]?(?:[1-9]\d*|0)(?![[:digit:]]))"
    R"())"
};
std::regex REG_KEYWORD {R"((AddHandler|AddressOf|Alias|And|AndAlso|As|Boolean|ByRef|Byte)"
                        R"(|ByVal|Call|Case|Catch|CBool|CByte|CChar|CDate|CDbl|CDec|Char)"
                        R"(|CInt|Class|CLng|CObj|Const|Continue|CSByte|CShort|CSng|CStr)"
                        R"(|CType|CUInt|CULng|CUShort|Date|Decimal|Declare|Default)"
                        R"(|Delegate|Dim|DirectCast|Do|Double|Each|Else|ElseIf|End|EndIf)"
                        R"(|Enum|Erase|Error|Event|Exit|False|Finally|For|For|Each…Next)"
                        R"(|Friend|Function|Get|GetType|GetXMLNamespace|Global|GoSub|GoTo)"
                        R"(|Handles|If|Implements|Imports|In|Inherits|Integer|Interface|Is)"
                        R"(|IsNot|Let|Lib|Like|Long|Loop|Me|Mod|Module|MustInherit)"
                        R"(|MustOverride|MyBase|MyClass|Namespace|Narrowing|New|Next|Not)"
                        R"(|Nothing|NotInheritable|NotOverridable|Object|Of|On|Operator)"
                        R"(|Option|Optional|Or|OrElse|Out|Overloads|Overridable|Overrides)"
                        R"(|ParamArray|Partial|Private|Property|Protected|Public)"
                        R"(|RaiseEvent|ReadOnly|ReDim|REM|RemoveHandler|Resume|Return)"
                        R"(|SByte|Select|Set|Shadows|Shared|Short|Single|Static|Step|Stop)"
                        R"(|String|Structure|Sub|SyncLock|Then|Throw|To|True|Try|TryCast)"
                        R"(|TypeOf…Is|UInteger|ULong|UShort|Using|Variant|Wend|When|While)"
                        R"(|Widening|With|WithEvents|WriteOnly|Xor|#Else)(?=\s))"};
std::regex REG_COMMENT {R"(('.*))"};
std::regex REG_STR_CHAR {R"(("(?:[^"]|"")*"))"};
std::regex REG_OPERATOR {
    // Await operator
    R"((Await)"
    // arithmetic and concatenation operators
    R"(|\^|\+|-|\*|\/|\|Mod|&|<<|>>)"
    // comparison operators
    R"(|=|<>|<|<=|>|>=|IsNot|Is|Like|TypeOf)"
    // logical and bitwise operators
    R"(|Not|And|AndAlso|Or|OrElse|Xor))"
};
std::regex REG_DELIMITER {R"(([;,:{}()\[\]'\\."_]))"};
std::regex REG_IDENTIFIER {R"(((?:_\w|[[:alpha:]])\w*))"};
std::regex REG_DIRECTIVE {
    R"((#Const\s.*)"
    R"(|#ExternalSource[\s\S]+#End ExternalSource)"
    R"(|#If .* Then[\s\S]+#End If)"
    R"(|#Region[\s\S]*#End Region.*)"
    R"(|#(Disable|Enable)\s[^\n]+((\n([[:blank:]]+[^\n]*)?)*\n[[:blank:]][^\n]*)?))"
};


class LexemeTable {
public:
    struct LexemeAppearance {
        LexemeType type;
        size_t position;
        size_t len;
    };

    bool addLexeme(LexemeType type, size_t position, size_t len) {

        if (len == 0) {
            throw std::out_of_range("Length of lexeme type " + std::to_string(type) +
                " is 0 (pos " + std::to_string(position) + ").");
        }

        // check for overlaps
        auto acceptable = std::all_of(table.begin(),
                    table.end(),
                    [position, len](LexemeAppearance la)->bool {
            return la.position >= position+len || la.position + la.len <= position;
        });

        if (! acceptable)
            return false;

        // add to the table
        table.push_back(LexemeAppearance {type, position, len});

        return true;
    }

    const std::vector<LexemeAppearance>& get_ordered_table() {
        // order the table
        std::sort(table.begin(),
                  table.end(),
                  [](const LexemeAppearance& la1, const LexemeAppearance& la2)
                  ->bool{
            return la1.position < la2.position;
        });

        return table;
    }

private:
    std::vector<LexemeAppearance> table {};

};


void analyze_vb_lexemes_helper(const std::string& vb_code, LexemeType type, LexemeTable &table);

LexemeTable analyze_vb_lexemes(const std::string& vb_code) {
    LexemeTable table;

    // analyze
    // preprocessor directives
    analyze_vb_lexemes_helper(vb_code, LexemeType::DIRECTIVE, table);

    // comments
    analyze_vb_lexemes_helper(vb_code, LexemeType::COMMENT, table);

    // strings
    analyze_vb_lexemes_helper(vb_code, LexemeType::STR_CHAR, table);

    // numbers
    analyze_vb_lexemes_helper(vb_code, LexemeType::NUMBER, table);

    // operators
    analyze_vb_lexemes_helper(vb_code, LexemeType::OPERATOR, table);

    // reserved words (keywords)
    analyze_vb_lexemes_helper(vb_code, LexemeType::KEYWORD, table);

    // identifiers
    analyze_vb_lexemes_helper(vb_code, LexemeType::IDENTIFIER, table);

    // delimiters
    analyze_vb_lexemes_helper(vb_code, LexemeType::DELIMITER, table);

    return table;
}

void analyze_vb_lexemes_helper(const std::string& vb_code, LexemeType type, LexemeTable &table) {
    std::regex regex;
    switch (type) {
        case LexemeType::NUMBER: {
            regex = REG_NUMBER;
            break;
        }
        case LexemeType::KEYWORD: {
            regex = REG_KEYWORD;
            break;
        }
        case LexemeType::COMMENT: {
            regex = REG_COMMENT;
            break;
        }
        case LexemeType::STR_CHAR: {
            regex = REG_STR_CHAR;
            break;
        }
        case LexemeType::OPERATOR: {
            regex = REG_OPERATOR;
            break;
        }
        case LexemeType::DELIMITER: {
            regex = REG_DELIMITER;
            break;
        }
        case LexemeType::IDENTIFIER: {
            regex = REG_IDENTIFIER;
            break;
        }
        case LexemeType::DIRECTIVE: {
            regex = REG_DIRECTIVE;
            break;
        }
    }

    std::regex_iterator<std::string::const_iterator> regexIterator
            {vb_code.begin(), vb_code.end(), regex};
    const std::regex_iterator<std::string::const_iterator> regexEnd{};

    for (;regexIterator != regexEnd; ++regexIterator) {
        table.addLexeme(type, regexIterator->position(1), regexIterator->length(1));
    }
}


enum BackgroundColor {
    RESET,
    RED,
    GREEN,
    YELLOW,
    BLUE,
    MAGENTA,
    CYAN,
    BRIGHT_CYAN,
    BRIGHT_GREEN
};

void set_background_color(BackgroundColor color) {
    std::string color_code;
    switch (color) {
        case BackgroundColor::RESET: {
            color_code = "0";
            break;
        }
        case BackgroundColor::RED: {
            color_code = "31";
            break;
        }
        case BackgroundColor::GREEN: {
            color_code = "32";
            break;
        }
        case BackgroundColor::BRIGHT_GREEN: {
            color_code = "102";
            break;
        }
        case BackgroundColor::YELLOW: {
            color_code = "43";
            break;
        }
        case BackgroundColor::BLUE: {
            color_code = "104";
            break;
        }
        case BackgroundColor::MAGENTA: {
            color_code = "45";
            break;
        }
        case BackgroundColor::CYAN: {
            color_code = "46";
            break;
        }
        case BackgroundColor::BRIGHT_CYAN: {
            color_code = "106";
            break;
        }
    }

    std::cout << "\033[" << color_code << 'm';
}


void set_background_for_lexeme(LexemeType type) {
    switch (type) {
        case LexemeType::NUMBER: {
            set_background_color(BackgroundColor::CYAN);
            break;
        }
        case LexemeType::KEYWORD: {
            set_background_color(BackgroundColor::YELLOW);
            break;
        }
        case LexemeType::COMMENT: {
            set_background_color(BackgroundColor::GREEN);
            break;
        }
        case LexemeType::STR_CHAR: {
            set_background_color(BackgroundColor::BRIGHT_CYAN);
            break;
        }
        case LexemeType::OPERATOR: {
            set_background_color(BackgroundColor::BLUE);
            break;
        }
        case LexemeType::DELIMITER: {
            set_background_color(BackgroundColor::MAGENTA);
            break;
        }
        case LexemeType::IDENTIFIER: {
            set_background_color(BackgroundColor::BRIGHT_GREEN);
            break;
        }
        case LexemeType::DIRECTIVE: {
            set_background_color(BackgroundColor::RED);
            break;
        }
    }
}


void print_highlighted_text(const std::string &text, LexemeTable &table) {
    auto &table_v = table.get_ordered_table();

    size_t next_pos = 0;
    for (auto item : table_v) {
        set_background_color(BackgroundColor::RESET);
        std::cout << text.substr(next_pos, item.position - next_pos);
        set_background_for_lexeme(item.type);
        std::cout << text.substr(item.position, item.len);
        next_pos = item.position + item.len;
    }
    set_background_color(BackgroundColor::RESET);
    std::cout << text.substr(next_pos);
}

void show_lexemes_colors() {
    std::cout << "Colors:\n";

    set_background_for_lexeme(LexemeType::COMMENT);
    std::cout << "Comment";
    set_background_color(BackgroundColor::RESET);
    std::cout << std::endl;

    set_background_for_lexeme(LexemeType::DIRECTIVE);
    std::cout << "Preprocessor directive";
    set_background_color(BackgroundColor::RESET);
    std::cout << std::endl;

    set_background_for_lexeme(LexemeType::DELIMITER);
    std::cout << "Delimiter";
    set_background_color(BackgroundColor::RESET);
    std::cout << std::endl;

    set_background_for_lexeme(LexemeType::OPERATOR);
    std::cout << "Operator";
    set_background_color(BackgroundColor::RESET);
    std::cout << std::endl;

    set_background_for_lexeme(LexemeType::KEYWORD);
    std::cout << "Keyword";
    set_background_color(BackgroundColor::RESET);
    std::cout << std::endl;

    set_background_for_lexeme(LexemeType::IDENTIFIER);
    std::cout << "Identifier";
    set_background_color(BackgroundColor::RESET);
    std::cout << std::endl;

    set_background_for_lexeme(LexemeType::STR_CHAR);
    std::cout << "String or character";
    set_background_color(BackgroundColor::RESET);
    std::cout << std::endl;

    set_background_for_lexeme(LexemeType::NUMBER);
    std::cout << "Number";
    set_background_color(BackgroundColor::RESET);
    std::cout << std::endl;

}

void print_usage(const std::string& programName) {
    std::cout << "Usage:\n" << programName << " vb_code_path\n";
}


int main(int argc, char *argv[]) {
    // read arguments
    const std::vector<std::string> args {argv, argv+argc};
    if (args.size() < 2 || args.size() > 3 || (args.size() == 3 && args[2] != "--verbose") ) {
        print_usage(args[0]);
        return EXIT_FAILURE;
    }
    bool verbose = argc == 3;

    std::string_view filePath {args[1]};

    // read source code
    std::ifstream codeFile (filePath, std::ios::binary);
    if (!codeFile.is_open()) {
        std::cout << "Cannot open the file.\n";
        return EXIT_FAILURE;
    }
    std::string code {(std::istreambuf_iterator<char>(codeFile)),
                      (std::istreambuf_iterator<char>())};
    codeFile.close();

    // analysis
    auto table = analyze_vb_lexemes(code);

    // show the result
    if (verbose) {
        show_lexemes_colors();
        std::cout << "\nResult:\n\n";
    }
    print_highlighted_text(code, table);


    return EXIT_SUCCESS;
}
