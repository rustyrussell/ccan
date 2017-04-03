#!/usr/bin/env python
# Some simple tests for the Python bindings for TDB
# Note that this tests the interface of the Python bindings
# It does not test tdb itself.
#
# Copyright (C) 2007-2013 Jelmer Vernooij <jelmer@samba.org>
# Published under the GNU LGPLv3 or later

import ntdb
from unittest import TestCase
import os, tempfile


class OpenTdbTests(TestCase):

    def test_nonexistent_read(self):
        self.assertRaises(IOError, ntdb.Ntdb, "/some/nonexistent/file", 0,
                ntdb.DEFAULT, os.O_RDWR)

class CloseTdbTests(TestCase):

    def test_double_close(self):
        self.ntdb = ntdb.Ntdb(tempfile.mkstemp()[1], ntdb.DEFAULT,
                           os.O_CREAT|os.O_RDWR)
        self.assertNotEqual(None, self.ntdb)

        # ensure that double close does not crash python
        self.ntdb.close()
        self.ntdb.close()

        # Check that further operations do not crash python
        self.assertRaises(RuntimeError, lambda: self.ntdb.transaction_start())

        self.assertRaises(RuntimeError, lambda: self.ntdb["bar"])


class InternalTdbTests(TestCase):

    def test_repr(self):
        self.ntdb = ntdb.Ntdb()

        # repr used to crash on internal db
        self.assertEquals(repr(self.ntdb), "Ntdb(<internal>)")


class SimpleTdbTests(TestCase):

    def setUp(self):
        super(SimpleTdbTests, self).setUp()
        self.ntdb = ntdb.Ntdb(tempfile.mkstemp()[1], ntdb.DEFAULT,
                           os.O_CREAT|os.O_RDWR)
        self.assertNotEqual(None, self.ntdb)

    def tearDown(self):
        del self.ntdb

    def test_repr(self):
        self.assertTrue(repr(self.ntdb).startswith("Ntdb('"))

    def test_lockall(self):
        self.ntdb.lock_all()

    def test_unlockall(self):
        self.ntdb.lock_all()
        self.ntdb.unlock_all()

    def test_lockall_read(self):
        self.ntdb.read_lock_all()
        self.ntdb.read_unlock_all()

    def test_store(self):
        self.ntdb.store("bar", "bla")
        self.assertEquals("bla", self.ntdb.get("bar"))

    def test_getitem(self):
        self.ntdb["bar"] = "foo"
        self.assertEquals("foo", self.ntdb["bar"])

    def test_delete(self):
        self.ntdb["bar"] = "foo"
        del self.ntdb["bar"]
        self.assertRaises(KeyError, lambda: self.ntdb["bar"])

    def test_contains(self):
        self.ntdb["bla"] = "bloe"
        self.assertTrue("bla" in self.ntdb)

    def test_keyerror(self):
        self.assertRaises(KeyError, lambda: self.ntdb["bla"])

    def test_name(self):
        self.ntdb.filename

    def test_iterator(self):
        self.ntdb["bla"] = "1"
        self.ntdb["brainslug"] = "2"
        l = list(self.ntdb)
        l.sort()
        self.assertEquals(["bla", "brainslug"], l)

    def test_transaction_cancel(self):
        self.ntdb["bloe"] = "2"
        self.ntdb.transaction_start()
        self.ntdb["bloe"] = "1"
        self.ntdb.transaction_cancel()
        self.assertEquals("2", self.ntdb["bloe"])

    def test_transaction_commit(self):
        self.ntdb["bloe"] = "2"
        self.ntdb.transaction_start()
        self.ntdb["bloe"] = "1"
        self.ntdb.transaction_commit()
        self.assertEquals("1", self.ntdb["bloe"])

    def test_transaction_prepare_commit(self):
        self.ntdb["bloe"] = "2"
        self.ntdb.transaction_start()
        self.ntdb["bloe"] = "1"
        self.ntdb.transaction_prepare_commit()
        self.ntdb.transaction_commit()
        self.assertEquals("1", self.ntdb["bloe"])

    def test_iterkeys(self):
        self.ntdb["bloe"] = "2"
        self.ntdb["bla"] = "25"
        i = self.ntdb.iterkeys()
        self.assertEquals(set(["bloe", "bla"]), set([i.next(), i.next()]))

    def test_clear(self):
        self.ntdb["bloe"] = "2"
        self.ntdb["bla"] = "25"
        self.assertEquals(2, len(list(self.ntdb)))
        self.ntdb.clear()
        self.assertEquals(0, len(list(self.ntdb)))

    def test_len(self):
        self.assertEquals(0, len(list(self.ntdb)))
        self.ntdb["entry"] = "value"
        self.assertEquals(1, len(list(self.ntdb)))

    def test_add_flags(self):
        self.ntdb.add_flag(ntdb.NOMMAP)
        self.ntdb.remove_flag(ntdb.NOMMAP)


class VersionTests(TestCase):

    def test_present(self):
        self.assertTrue(isinstance(ntdb.__version__, str))


if __name__ == '__main__':
    import unittest
    unittest.TestProgram()
