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

# Run with pytest --capture=tee-sys -v ... to see stdout
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

  [    73]  300-Mil l3-log msgs
  Trailer line should be skipped
                    """

    (string_offs, nlines) = l3_dump.parse_string_offsets(test_ro_data)

    assert nlines == 2

    exp_hash = { 115: '300-Mil l3-log msgs', 381: 'test string' }

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
  [    21]  Exercise in-memory logging performance benchmarking: %d Mil log msgs\n
  [    b9]  300-Mil l3-log msgs
  [    d8]  %d Mil log msgs: %luns/msg (avg)\n
  [   101]  /tmp/l3.c-test.dat
  [   11a]  Log-msg-Args(1,2)
  [   138]  Potential memory overwrite (addr, size)
  [   160]  Invalid buffer handle (addr)
  [   17d]  test string"""

    (string_offs, nlines) = l3_dump.parse_string_offsets(test_ro_data)

    assert nlines == 9

    exp_hash = {    8: '/tmp/l3.c-test.dat'
                 , 33: 'Exercise in-memory logging performance benchmarking:'
                        + ' %d Mil log msgs'
                , 185: '300-Mil l3-log msgs'
                , 216: '%d Mil log msgs: %luns/msg (avg)'
                , 257: '/tmp/l3.c-test.dat'
                , 282: 'Log-msg-Args(1,2)'
                , 312: 'Potential memory overwrite (addr, size)'
                , 352: 'Invalid buffer handle (addr)'
                , 381: 'test string'
               }

    assert string_offs == exp_hash

# #############################################################################
def test_fmtstr_replace():
    """
    Verify the format-string parameter replacement done by the L3-dump utility.
    """

    fmtstr = "This has an embedded pct-p like this %p"
    expfmt = "This has an embedded pct-p like this 0x%x"
    assert expfmt == l3_dump.fmtstr_replace(fmtstr)

    fmtstr = "This has an embedded pct-p like this 0x%p"
    expfmt = "This has an embedded pct-p like this 0x%x"
    assert expfmt == l3_dump.fmtstr_replace(fmtstr)

    fmtstr = "Up to 2 instances of pct-p %p and 0x-pct-p 0x%p should be replaced"
    expfmt = "Up to 2 instances of pct-p 0x%x and 0x-pct-p 0x%x should be replaced"
    assert expfmt == l3_dump.fmtstr_replace(fmtstr)

    # As fmtstr_replace() does a pair of replacement, we can effectively support
    # up to 2 pairs of such format specifiers
    fmtstr = "More than 2 instances of pct-p %p and 0x-pct-p 0x%p %p can also be replaced"
    expfmt = "More than 2 instances of pct-p 0x%x and 0x-pct-p 0x%x 0x%x can also be replaced"
    assert expfmt == l3_dump.fmtstr_replace(fmtstr)

    fmtstr = "More than 2 instances of pct-p %p and %p and %p remain un-replaced."
    expfmt = "More than 2 instances of pct-p 0x% and 0x% and 0x%x remain un-replaced."
    assert expfmt != l3_dump.fmtstr_replace(fmtstr)

    fmtstr = "This has a simple pct-x which should be unchanged %x"
    expfmt = "This has a simple pct-x which should be unchanged %x"
    assert expfmt == l3_dump.fmtstr_replace(fmtstr)

    fmtstr = "This has a simple 0x-pct-x which should be unchanged 0x%x"
    expfmt = "This has a simple 0x-pct-x which should be unchanged 0x%x"
    assert expfmt == l3_dump.fmtstr_replace(fmtstr)

# #############################################################################
def test_fmtstr_replace_and_print():
    """
    Verify the format-string parameter replacement done by the L3-dump utility.
    Then, invoke the print() on supplied arguments, to ensure that it will work.
    This goes thru the code-flow when user has invoked L3-log utility with a
    message that has print format-specifiers like '%u', '%lu', '%llu' and are
    printing unsigned ints.
    """

    fmtstr = "This has an embedded pct-u for unsigned int=%u"
    expfmt = "This has an embedded pct-u for unsigned int=%d"
    printfmt = l3_dump.fmtstr_replace(fmtstr)
    assert expfmt == printfmt
    print(printfmt % (1))

    fmtstr = "This has an embedded pct-lu for unsigned int=%lu"
    expfmt = "This has an embedded pct-lu for unsigned int=%d"
    printfmt = l3_dump.fmtstr_replace(fmtstr)
    assert expfmt == printfmt
    number = int('0x80000000', 16)
    print(printfmt % (number))

    fmtstr = "This has an embedded pct-llu for unsigned int=%llu"
    expfmt = "This has an embedded pct-llu for unsigned int=%d"
    printfmt = l3_dump.fmtstr_replace(fmtstr)
    assert expfmt == printfmt
    # Print a Very large unsigned long-long-int
    number = int('0x8000' + '0000' + '0000' + '0000', 16)
    print(printfmt % (number))

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
