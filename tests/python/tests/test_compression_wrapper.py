import unittest
import os.path
import createrepo_c as cr

from fixtures import *

class TestCaseCompressionWrapper(unittest.TestCase):
    def test_compression_suffix(self):
        self.assertEqual(cr.compression_suffix(cr.AUTO_DETECT_COMPRESSION), None)
        self.assertEqual(cr.compression_suffix(cr.UNKNOWN_COMPRESSION), None)
        self.assertEqual(cr.compression_suffix(cr.NO_COMPRESSION), None)
        self.assertEqual(cr.compression_suffix(123), None)

        self.assertEqual(cr.compression_suffix(cr.GZ), ".gz")
        self.assertEqual(cr.compression_suffix(cr.BZ2), ".bz2")
        self.assertEqual(cr.compression_suffix(cr.XZ), ".xz")

    def test_detect_compression(self):

        # no compression
        path = os.path.join(COMPRESSED_FILES_PATH, "01_plain.txt")
        comtype = cr.detect_compression(path)
        self.assertEqual(comtype, cr.NO_COMPRESSION)

        # gz compression
        path = os.path.join(COMPRESSED_FILES_PATH, "01_plain.txt.gz")
        comtype = cr.detect_compression(path)
        self.assertEqual(comtype, cr.GZ)

        # bz2 compression
        path = os.path.join(COMPRESSED_FILES_PATH, "01_plain.txt.bz2")
        comtype = cr.detect_compression(path)
        self.assertEqual(comtype, cr.BZ2)

        # xz compression
        path = os.path.join(COMPRESSED_FILES_PATH, "01_plain.txt.xz")
        comtype = cr.detect_compression(path)
        self.assertEqual(comtype, cr.XZ)

        # Bad suffix - no compression
        path = os.path.join(COMPRESSED_FILES_PATH, "01_plain.foo0")
        comtype = cr.detect_compression(path)
        self.assertEqual(comtype, cr.NO_COMPRESSION)

        # Bad suffix - gz compression
        path = os.path.join(COMPRESSED_FILES_PATH, "01_plain.foo1")
        comtype = cr.detect_compression(path)
        self.assertEqual(comtype, cr.GZ)

        # Bad suffix - bz2 compression
        path = os.path.join(COMPRESSED_FILES_PATH, "01_plain.foo2")
        comtype = cr.detect_compression(path)
        self.assertEqual(comtype, cr.BZ2)

        # Bad suffix - xz compression
        path = os.path.join(COMPRESSED_FILES_PATH, "01_plain.foo3")
        comtype = cr.detect_compression(path)
        self.assertEqual(comtype, cr.XZ)

