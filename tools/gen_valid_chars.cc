// generate the atlas valid charsets

#include <array>
#include <fstream>

void dump_array(std::ostream& os, const std::string& name,
                const std::array<char, 256>& chars) {
  os << "static constexpr std::array<char, 256> " << name << " = {{";

  os << "'" << chars[0] << "'";
  for (auto i = 1u; i < chars.size(); ++i) {
    os << ", '" << chars[i] << "'";
  }

  os << "}};\n";
}

int main(int argc, char* argv[]) {
  std::ofstream of;
  if (argc > 1) {
    of.open(argv[1]);
  } else {
    of.open("/dev/stdout");
  }

  std::array<char, 256> charsAllowed{};
  for (int i = 0; i < 256; ++i) {
    charsAllowed[i] = '_';
  }

  charsAllowed['.'] = '.';
  charsAllowed['-'] = '-';

  for (auto ch = '0'; ch <= '9'; ++ch) {
    charsAllowed[ch] = ch;
  }
  for (auto ch = 'a'; ch <= 'z'; ++ch) {
    charsAllowed[ch] = ch;
  }
  for (auto ch = 'A'; ch <= 'Z'; ++ch) {
    charsAllowed[ch] = ch;
  }
  charsAllowed['~'] = '~';
  charsAllowed['^'] = '^';

  dump_array(of, "kAtlasChars", charsAllowed);
}
