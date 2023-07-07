# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# This script generates the file convert_type.cl, which contains all of the
# OpenCL functions in the form:
#
# convert_<destTypen><_sat><_roundingMode>(<sourceTypen>)

types = ['char', 'uchar', 'short', 'ushort', 'int', 'uint', 'float']
int_types = ['char', 'uchar', 'short', 'ushort', 'int', 'uint']
unsigned_types = ['uchar', 'ushort', 'uint']
float_types = ['float']
vector_sizes = ['', '2', '3', '4', '8', '16']
half_sizes = [('2',''), ('4','2'), ('8','4'), ('16','8')]

saturation = ['','_sat']
rounding_modes = ['_rtz','_rte','_rtp','_rtn']
float_prefix = {'float':'FLT_', 'double':'DBL_'}
float_suffix = {'float':'f'}

bool_type = {'char'  : 'char',
             'uchar' : 'char',
             'short' : 'short',
             'ushort': 'short',
             'int'   : 'int',
             'uint'  : 'int',
             'float'  : 'int'}

unsigned_type = {'char'  : 'uchar',
                 'uchar' : 'uchar',
                 'short' : 'ushort',
                 'ushort': 'ushort',
                 'int'   : 'uint',
                 'uint'  : 'uint'}

sizeof_type = {'char'  : 1, 'uchar'  : 1,
               'short' : 2, 'ushort' : 2,
               'int'   : 4, 'uint'   : 4,
               'float' : 4}

limit_max = {'char'  : 'CHAR_MAX',
             'uchar' : 'UCHAR_MAX',
             'short' : 'SHRT_MAX',
             'ushort': 'USHRT_MAX',
             'int'   : 'INT_MAX',
             'uint'  : 'UINT_MAX',
             'long'  : 'LONG_MAX',
             'ulong' : 'ULONG_MAX'}

limit_min = {'char'  : 'CHAR_MIN',
             'uchar' : '0',
             'short' : 'SHRT_MIN',
             'ushort': '0',
             'int'   : 'INT_MIN',
             'uint'  : '0',
             'long'  : 'LONG_MIN',
             'ulong' : '0'}

def conditional_guard(src, dst):
  int64_count = 0
  float64_count = 0
  # if src in int64_types:
  #   int64_count = int64_count +1
  # elif src in float64_types:
  #   float64_count = float64_count + 1
  # if dst in int64_types:
  #   int64_count = int64_count +1
  # elif dst in float64_types:
  #   float64_count = float64_count + 1
  if float64_count > 0:
    #In embedded profile, if cl_khr_fp64 is supported cles_khr_int64 has to be
    print("#ifdef cl_khr_fp64")
    return True
  elif int64_count > 0:
    print("#if defined cles_khr_int64 || !defined(__EMBEDDED_PROFILE__)")
    return True
  return False