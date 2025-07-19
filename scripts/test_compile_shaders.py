from compile_shaders import TokenType, lex_string
import unittest

class TestLexer(unittest.TestCase):

    def test_version(self):
        s = "#version {{ VERSION }}\n"
        ts = lex_string(s)
        self.assertEqual(ts[0], TokenType.POUND)
        self.assertEqual(ts[1], TokenType.VERSION)
        self.assertEqual(ts[2], TokenType.DOUBLE_L_BRACE)
        self.assertEqual(ts[3], TokenType.DIRECTIVE_VERSION)
        self.assertEqual(ts[4], TokenType.DOUBLE_R_BRACE)

if __name__ == "__main__":
    unittest.main()
