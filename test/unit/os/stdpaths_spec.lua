local helpers = require('test.unit.helpers')(after_each)
local itp = helpers.gen_itp(it)

local cimport = helpers.cimport
local get_str = helpers.ffi.string
local eq = helpers.eq
local cstr = helpers.cstr
local to_cstr = helpers.to_cstr
local NULL = helpers.NULL
local OK = 0

require('lfs')

local cimp = cimport('./src/nvim/os/os.h')

describe('stdpaths.c', function()
  local function remove_duplicate_directories(str)
    return cimp.remove_duplicate_directories(to_cstr(str))
  end

  itp('remove_duplicate_directories', function()
   input = '/usr/local/share:/usr/share:/usr/share'
   expected = '/usr/local/share:/usr/share'

   eq(expected, get_str(remove_duplicate_directories(input)))

   input = 'a/b/c:abc/def:a/bcdef:abc/def:a:a/bcdef:a/b/c'
   expected = 'a/b/c:abc/def:a/bcdef:a'

   eq(expected, get_str(remove_duplicate_directories(input)))

   input = '/usr/local/share:/usr/share'
   expected = '/usr/local/share:/usr/share'

   eq(expected, get_str(remove_duplicate_directories(input)))

   input = '/usr/local/share'
   expected = '/usr/local/share'

   eq(expected, get_str(remove_duplicate_directories(input)))
  end)
end)
