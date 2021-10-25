import sys
import os
import unittest

sys.path.append(os.path.normpath(os.path.join(os.path.dirname(__file__), os.path.pardir)))

import quast_libs.qutils as qq


class TestCheckDirPath(unittest.TestCase):
    def test_check_wrong_format(self):
        s = "♥O◘♦♥O◘♦"
        with self.assertRaises(SystemExit):
            qq.check_dirpath(s)

    def test_check_spaces(self):
        s = " misha@misha:~$"
        with self.assertRaises(SystemExit):
            qq.check_dirpath(s)

    def test_check_right_format(self):
        s = "misha@misha:~$"
        self.assertTrue(qq.check_dirpath(s))


if __name__ == '__main__':
    unittest.main()
