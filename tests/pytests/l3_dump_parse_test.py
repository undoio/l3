# #############################################################################
# l3_dump_parse_test: Testing parsing methods in l3_dump.py
#
"""
Basic unit-tests for methods in l3_dump.py. These tests exercise the core
unpacking methods in this script using standalone data, to verify that core
logic parsing `readelf` output works correctly.
"""
import os
import sys

# #############################################################################
# Full dir-path where this tests/ dir lives
L3PyTestsDir    = os.path.realpath(os.path.dirname(__file__))
L3RootDir       = os.path.abspath(L3PyTestsDir + '/../..')

sys.path.append(L3RootDir)

# pylint: disable-msg=import-error,wrong-import-position
import l3_dump

# #############################################################################
def test_parse_rodata_start_offset_basic():
    """
    Exercise basic parsing of parse_rodata_start_offset()
    """
    test_ro_data_hex_dump = """0x00002000 01000200 00000000 2f746d70 2f6c332e ......../tmp/l3."""
    offset = l3_dump.parse_rodata_start_offset(test_ro_data_hex_dump)

    exp_offset = int('0x00002000', 16)
    assert offset == exp_offset

# #############################################################################
def test_parse_rodata_start_offset_leading_spaces():
    """
    Exercise basic parsing of parse_rodata_start_offset()
    """
    test_ro_data_hex_dump = """  0x00002000 01000200 00000000 2f746d70 2f6c332e ......../tmp/l3."""
    offset = l3_dump.parse_rodata_start_offset(test_ro_data_hex_dump)

    exp_offset = int('0x00002000', 16)
    assert offset == exp_offset

# #############################################################################
def test_parse_rodata_start_offset_no_leading_hex_digits():
    """
    Exercise basic parsing of parse_rodata_start_offset() without leading '0x'
    """
    test_ro_data_hex_dump = """00002000 01000200 00000000 2f746d70 2f6c332e ......../tmp/l3."""
    offset = l3_dump.parse_rodata_start_offset(test_ro_data_hex_dump)

    exp_offset = int('0x00002000', 16)
    assert offset == exp_offset

# #############################################################################
def test_parse_rodata_start_offset_with_spaces_and_no_leading_hex_digits():
    """
    Exercise basic parsing of parse_rodata_start_offset() without leading '0x'
    """
    test_ro_data_hex_dump = """  00002000 01000200 00000000 2f746d70 2f6c332e ......../tmp/l3."""
    offset = l3_dump.parse_rodata_start_offset(test_ro_data_hex_dump)

    exp_offset = int('0x00002000', 16)
    assert offset == exp_offset

# #############################################################################
def test_parse_rodata_start_offset_readelf_dump():
    """
    Exercise basic parsing of parse_rodata_start_offset() using an input that
    is exactly what Linux `readelf` spits out.
    """
    test_ro_data_hex_dump = """Hex dump of section '.rodata':
  0x00002000 01000200 00000000 2f746d70 2f6c332e ......../tmp/l3.
  """
    offset = l3_dump.parse_rodata_start_offset(test_ro_data_hex_dump)

    exp_offset = int('0x00002000', 16)
    assert offset == exp_offset

# #############################################################################
def test_basic_parse_rodata_string_offsets():
    """
    Cross-check LOC-encoded value for few hard-coded inputs.
    """
    test_ro_data = """[   17d]  test string"""

    (string_offs, nlines) = l3_dump.parse_string_offsets(test_ro_data)

    assert nlines == 1

    exp_hash = { 381: 'test string' }

    pr_debug_info(test_ro_data, string_offs, exp_hash)

    assert string_offs == exp_hash

# #############################################################################
def test_basic_parse_rodata_string_offsets_skip_junk_lines():
    """
    Cross-check LOC-encoded value for few hard-coded inputs. Inject some junk
    leader / trailer lines, and verify that they are skipped.
    """
    test_ro_data = """String dump of section '.rodata':
  [   17d]  test string
  Unrelated junk-lines should be skipped
  Blank line below should be skipped

  [    73]  300-Mil Fast l3-log msgs
  Trailer line should be skipped
                    """

    (string_offs, nlines) = l3_dump.parse_string_offsets(test_ro_data)

    assert nlines == 2

    exp_hash = { 115: '300-Mil Fast l3-log msgs', 381: 'test string' }

    pr_debug_info(test_ro_data, string_offs, exp_hash)

    assert string_offs == exp_hash

# #############################################################################
def test_basic_parse_rodata_string_offsets_with_varying_blanks():
    """
    Verify parsing r.e. that it correctly skips all variations in spaces.
    The data is designed such that the offsets keep varying. Otherwise, the
    parsing routine that builds-and-returns a hash, will eliminate duplicate
    keys.
    """
    test_ro_data = """
[100] test string
 [101] test string
[ 102] test string
[103 ] test string
 [104] test string
 [ 105] test string
 [ 106 ] test string
[ 107 ]  test string
  [   108  ]   test string
"""

    (string_offs, nlines) = l3_dump.parse_string_offsets(test_ro_data)

    assert nlines == 9

    exp_hash = {  256: 'test string'
                , 257: 'test string'
                , 258: 'test string'
                , 259: 'test string'
                , 260: 'test string'
                , 261: 'test string'
                , 262: 'test string'
                , 263: 'test string'
                , 264: 'test string'
               }

    pr_debug_info(test_ro_data, string_offs, exp_hash)

    assert string_offs == exp_hash

# #############################################################################
def test_parse_rodata_string_offsets():
    """
    Cross-check LOC-encoded value for few hard-coded inputs.
    This hard-coded .rodata output string is the one obtained from one of
    the unit-tests.
    """
    test_ro_data = """String dump of section '.rodata':
  [     8]  /tmp/l3.c-test.dat
  [    21]  Exercise in-memory logging performance benchmarking: %d Mil simple/fast log msgs\n
  [    73]  300-Mil Fast l3-log msgs
  [    90]  %d Mil fast log msgs  : %luns/msg (avg)\n
  [    b9]  300-Mil Simple l3-log msgs
  [    d8]  %d Mil simple log msgs: %luns/msg (avg)\n
  [   101]  /tmp/l3.c-small-test.dat
  [   11a]  Simple-log-msg-Args(1,2)
  [   138]  Potential memory overwrite (addr, size)
  [   160]  Invalid buffer handle (addr)
  [   17d]  test string"""

    (string_offs, nlines) = l3_dump.parse_string_offsets(test_ro_data)

    assert nlines == 11

    exp_hash = {    8: '/tmp/l3.c-test.dat'
                 , 33: 'Exercise in-memory logging performance benchmarking:'
                        + ' %d Mil simple/fast log msgs'
                , 115: '300-Mil Fast l3-log msgs'
                , 144: '%d Mil fast log msgs  : %luns/msg (avg)'
                , 185: '300-Mil Simple l3-log msgs'
                , 216: '%d Mil simple log msgs: %luns/msg (avg)'
                , 257: '/tmp/l3.c-small-test.dat'
                , 282: 'Simple-log-msg-Args(1,2)'
                , 312: 'Potential memory overwrite (addr, size)'
                , 352: 'Invalid buffer handle (addr)'
                , 381: 'test string'
               }
    assert string_offs == exp_hash

# #############################################################################
def pr_debug_info(ro_data:str, string_offs:dict, exp_hash:dict):
    """
    Debug routine: Print test ro_data string and the parsed string_offsets hash.
    """
    print(ro_data)

    print("\nExpected string-offsets:\n")
    for offset in exp_hash.keys():
        print(f"{offset=} -> {exp_hash[offset]}")

    print("\nActual string-offsets:\n")
    for offset in string_offs.keys():
        print(f"{offset=} -> {string_offs[offset]}")
