#include "goodjson.h"

#include <cstdlib>
#include <cstring>
#include <stdio.h>

#ifdef _WIN32

#include <windows.h>
#include <intrin.h>

#pragma intrinsic(_BitScanReverse64)

//---------------------------------------------------------------------------------
inline uint32_t gj_Bsr( uint64_t val )
{
  DWORD idx;
  bool found = _BitScanReverse64( &idx, val );
  return found ? (uint32_t)idx : (uint32_t)-1;
}

#endif

//---------------------------------------------------------------------------------
static constexpr size_t kUnreasonablyLargeStringSize = 1 * 1024 * 1024; // 1 MB limit on string sizes

//---------------------------------------------------------------------------------
// including null terminator
size_t gj_StrLen( const char* str )
{
  // todo: vectorize this?
  uint32_t idx = 0;
  while ( str[ idx ] != '\0' && idx != kUnreasonablyLargeStringSize) { idx++; }
  
  return idx == kUnreasonablyLargeStringSize ? (size_t)-1 : idx;
}

// Crc32 functionality from https://github.com/stbrumme/crc32 under zlib license
// has been modified to suit coding standard of goodjson
constexpr uint32_t kCrc32Lookup[256] = {
  0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
  0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
  0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
  0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
  0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
  0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
  0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
  0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
  0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
  0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
  0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
  0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
  0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
  0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
  0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
  0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
  0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,
  0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
  0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,
  0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
  0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
  0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
  0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,
  0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
  0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
  0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
  0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
  0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
  0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,
  0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
  0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,
  0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D,
};

//---------------------------------------------------------------------------------
uint32_t gj_crc32( const char* str )
{
  uint32_t length = (uint32_t)gj_StrLen( str );

  uint32_t crc = 0;
  const uint8_t* current = (const uint8_t*)str;

  while (length-- != 0)
    crc = (crc >> 8) ^ kCrc32Lookup[(crc & 0xFF) ^ *current++];

  return ~crc; // same as crc ^ 0xFFFFFFFF
}

//---------------------------------------------------------------------------------
static gjMallocFn s_MallocFn = nullptr;
static gjFreeFn   s_FreeFn   = nullptr;
static gjAssertFn s_AssertFn = nullptr;
static gjConfig   s_Config;

//---------------------------------------------------------------------------------
void gj_setAllocator( const gjAllocatorHooks* hooks )
{
  s_MallocFn = hooks->mallocFn;
  s_FreeFn   = hooks->freeFn;
}

//---------------------------------------------------------------------------------
void gj_setAssertFn( gjAssertFn assert_fn )
{
  s_AssertFn = assert_fn;
}

//---------------------------------------------------------------------------------
void* gj_malloc( size_t sz, const char* description )
{
  if ( s_MallocFn != nullptr )
  {
    return s_MallocFn( sz, description );
  }

  return malloc( sz );
}

//---------------------------------------------------------------------------------
void gj_free( void* ptr )
{
  if ( s_FreeFn != nullptr )
  {
    s_FreeFn( ptr );
  }
  else
  {
    free( ptr );
  }
}

//---------------------------------------------------------------------------------
void gj_assert( const char* message )
{
  if ( s_AssertFn != nullptr )
  {
    s_AssertFn( message );
  }
  else
  {
    printf( message );
    printf( "\n\n" );
    __debugbreak();
  }
}

//---------------------------------------------------------------------------------
struct _gjMemberHandle
{
  uint32_t m_Idx;
  uint32_t m_Gen;
};

//---------------------------------------------------------------------------------
static constexpr uint32_t kMemberIdxTail = (uint32_t)-1;
struct _gjMember
{
  char*    m_KeyStr;
  gjValue  m_Value;
  uint32_t m_KeyHash;
  uint32_t m_Gen;
  uint32_t m_Next;
};

//---------------------------------------------------------------------------------
struct _gjArrayHandle
{
  uint32_t m_Idx;
  uint32_t m_Gen;
};

//---------------------------------------------------------------------------------
static constexpr uint32_t kArrayIdxTail = (uint32_t)-1;
struct _gjArrayElem
{
  gjValue  m_Value;
  uint32_t m_Gen;
  uint32_t m_Next;
};

//---------------------------------------------------------------------------------
enum gjSubValueType : uint8_t
{
  kGjSubValueTypeU64,
  kGjSubValueTypeInt,
  kGjSubValueTypeFloat,

  kGjSubValueTypeCount,
  kGjSubValueTypeInvalid = (uint8_t)-1
};

#define VAL_TYPE( value ) (gjValueType)(value->m_TypeGroup & 0x07)
#define VAL_SUBTYPE( value ) (gjSubValueType)((value->m_TypeGroup & 0xf8) >> 3 )

#define ASSIGN_VAL_TYPE( value, type ) value->m_TypeGroup &= ~0x07; value->m_TypeGroup |= ((uint8_t)type & 0x07);
#define ASSIGN_VAL_SUBTYPE( value, type ) value->m_TypeGroup &= ~0xf8; value->m_TypeGroup |= (((uint8_t)type & 0x1f) << 3);

//---------------------------------------------------------------------------------
struct _gjValue
{
  union
  {
    float           m_Float;
    uint64_t        m_U64;
    int             m_Int;
    char*           m_Str;
    bool            m_Bool;
    _gjArrayHandle  m_ArrayStart;
    _gjMemberHandle m_ObjectStart;
  };
  uint32_t       m_Gen;
  uint8_t        m_TypeGroup;
};

//---------------------------------------------------------------------------------
void*         s_InitialDynamicBacking;
_gjValue*     s_ValuePool;
uint64_t*     s_ValueBitset;
uint32_t      s_ValueBitsetWordCount;
_gjArrayElem* s_ArrayPool;
uint32_t      s_ArrayPoolHead;
_gjMember*    s_MemberPool;
uint32_t      s_MemberPoolHead;

//---------------------------------------------------------------------------------
gjConfig gj_getDefaultConfig()
{
  gjConfig config;
  config.max_value_count  =  4096;
  return config;
}

//---------------------------------------------------------------------------------
void gj_init( const gjConfig* config )
{
  s_Config = *config;
  
  s_ValueBitsetWordCount = ( config->max_value_count >> 6 ) + ((config->max_value_count & 0x0000003f) != 0);

  const size_t value_pool_sz   = config->max_value_count * sizeof ( *s_ValuePool );
  const size_t value_bitset_sz = s_ValueBitsetWordCount  * sizeof( uint64_t );
  const size_t array_pool_sz   = config->max_value_count * sizeof( *s_ArrayPool );
  const size_t member_pool_sz  = config->max_value_count * sizeof( *s_MemberPool );

  const size_t total_sz = value_pool_sz
                        + value_bitset_sz
                        + array_pool_sz
                        + member_pool_sz;

  s_InitialDynamicBacking = (_gjValue*)gj_malloc( total_sz, "gj: initial bookkeepping alloc" );
  void* cursor = s_InitialDynamicBacking;

  s_ValuePool   = (_gjValue*)    cursor; cursor = ((uint8_t*)cursor + value_pool_sz  );
  s_ValueBitset = (uint64_t*)    cursor; cursor = ((uint8_t*)cursor + value_bitset_sz);
  s_ArrayPool   = (_gjArrayElem*)cursor; cursor = ((uint8_t*)cursor + array_pool_sz  );
  s_MemberPool  = (_gjMember*)   cursor; cursor = ((uint8_t*)cursor + member_pool_sz );

  memset( s_ValuePool,   0, value_pool_sz   );
  memset( s_ValueBitset, 0, value_bitset_sz );
  memset( s_ArrayPool,   0, array_pool_sz   );
  memset( s_MemberPool,  0, member_pool_sz  );

  for ( uint32_t i_elem = 0; i_elem < config->max_value_count; ++i_elem )
  {
    s_ArrayPool [ i_elem ].m_Next = i_elem + 1;
    s_MemberPool[ i_elem ].m_Next = i_elem + 1;
  }
  s_ArrayPool [ config->max_value_count - 1 ].m_Next = kArrayIdxTail;
  s_MemberPool[ config->max_value_count - 1 ].m_Next = kMemberIdxTail;
  s_ArrayPoolHead  = 0;
  s_MemberPoolHead = 0;
}

//---------------------------------------------------------------------------------
void gj_shutdown()
{
  gj_free( s_InitialDynamicBacking );
}

//---------------------------------------------------------------------------------
_gjValue* gj_allocValue( uint32_t* out_idx )
{
  *out_idx = (uint32_t)-1;
  for ( uint32_t i_word = 0; i_word < s_ValueBitsetWordCount; ++i_word )
  {
    const uint32_t first_bit = gj_Bsr( ~s_ValueBitset[ i_word ] );
    if ( first_bit != (uint32_t)-1 )
    {
      *out_idx = i_word * 64 + 63 - first_bit;
      break;
    }
  }

  if ( *out_idx != (uint32_t)-1 && *out_idx < s_Config.max_value_count )
  {
    const uint32_t set_idx = (*out_idx) >> 0x6;
    const uint64_t bit = ( 0x8000000000000000 >> ( (*out_idx) & 0x3f));
    s_ValueBitset[ set_idx ] |= bit;
    return &s_ValuePool[ *out_idx ];
  }

  gj_assert( "Ran out of slots for values" );
  return nullptr;
}

//---------------------------------------------------------------------------------
void gj_freeValue( uint32_t idx )
{
  if ( idx < s_Config.max_value_count )
  {
    const uint32_t set_idx = idx >> 0x6;
    const uint64_t bit = ( 0x8000000000000000 >> ( idx & 0x3f ) );
    s_ValueBitset[ set_idx ] &= ~bit;
    s_ValuePool[ idx ].m_Gen++;
  }
}

//---------------------------------------------------------------------------------
_gjArrayElem* gj_allocArrayElem( uint32_t* inout_head_idx, uint32_t array_idx= kArrayIndexEnd )
{
  if ( s_ArrayPoolHead != kArrayIdxTail )
  {
    if ( *inout_head_idx == kArrayIdxTail || array_idx == 0 )
    {
      *inout_head_idx = s_ArrayPoolHead;
      s_ArrayPoolHead = s_ArrayPool[ s_ArrayPoolHead ].m_Next;
      s_ArrayPool[ *inout_head_idx ].m_Next = kArrayIdxTail;
      return &s_ArrayPool[ *inout_head_idx ];
    }
    else
    {
      _gjArrayElem* elem = &s_ArrayPool[ *inout_head_idx ];
      uint32_t      idx  = 1;
      while ( elem->m_Next != kArrayIdxTail && idx++ != array_idx )
      {
        elem = &s_ArrayPool[ elem->m_Next ];
      }
      const uint32_t new_idx = s_ArrayPoolHead;
      s_ArrayPoolHead = s_ArrayPool[ s_ArrayPoolHead ].m_Next;
      
      const uint32_t prev_next = elem->m_Next;
      elem->m_Next = new_idx;
      s_ArrayPool[ new_idx ].m_Next = prev_next;

      return &s_ArrayPool[ new_idx ];
    }
  }

  return nullptr;
}

//---------------------------------------------------------------------------------
// returns the freed elem
_gjArrayElem* gj_freeArrayElem( _gjArrayHandle* inout_head_handle, uint32_t array_idx )
{
  if ( inout_head_handle->m_Idx < s_Config.max_value_count )
  {
    _gjArrayElem* elem = &s_ArrayPool[ inout_head_handle->m_Idx ];
    if ( elem->m_Gen == inout_head_handle->m_Gen )
    {
      if ( array_idx == 0 )
      {
        const uint32_t prev_head = s_ArrayPoolHead;
        s_ArrayPoolHead = inout_head_handle->m_Idx;

        inout_head_handle->m_Idx = elem->m_Next;
        inout_head_handle->m_Gen = inout_head_handle->m_Idx != kArrayIdxTail
                                 ? s_ArrayPool[ inout_head_handle->m_Idx ].m_Gen
                                 : (uint32_t)-1;
        elem->m_Gen++;
        elem->m_Next = prev_head;

        return elem;
      }
      else
      {
        uint32_t idx = 1;
        while ( idx++ != array_idx && elem->m_Next != kArrayIdxTail )
        {
          elem = &s_ArrayPool[ elem->m_Next ];
        }

        if ( idx - 1 == array_idx )
        {
          const uint32_t free_idx = elem->m_Next;
          elem->m_Next = s_ArrayPool[ elem->m_Next ].m_Next;
          s_ArrayPool[ free_idx ].m_Next = s_ArrayPoolHead;
          s_ArrayPool[ free_idx ].m_Gen++;
          s_ArrayPoolHead = free_idx;

          return &s_ArrayPool[ free_idx ];
        }
        else
        {
          gj_assert( "Attempted to free array elem that does not exist in provided head list. This is a problem with the library and should be reported to the github with a minimal repro." );
        }
      }
    }
    else
    {
      gj_assert( "Attempting to free a member that has already been freed" );
    }
  }
  else
  {
    gj_assert( "Attempting to free a member that has an invalid handle" );
  }

  return nullptr;
}

//---------------------------------------------------------------------------------
void gj_freeArrayElemList( _gjArrayHandle head_handle )
{
  if ( head_handle.m_Idx < s_Config.max_value_count )
  {
    _gjArrayElem* elem = &s_ArrayPool[ head_handle.m_Idx ];
    if ( elem->m_Gen == head_handle.m_Gen )
    {
      elem->m_Gen++;
      while ( elem->m_Next != kArrayIdxTail )
      {
        elem = &s_ArrayPool[ elem->m_Next ];
        elem->m_Gen++;
      }
      elem->m_Next    = s_ArrayPoolHead;
      s_ArrayPoolHead = head_handle.m_Idx; 
    }
  }
}

//---------------------------------------------------------------------------------
_gjMember* gj_allocMember( uint32_t* inout_head )
{
  if ( s_MemberPoolHead != kMemberIdxTail )
  {
    if ( *inout_head == kMemberIdxTail )
    {
      *inout_head = s_MemberPoolHead;
      s_MemberPoolHead = s_MemberPool[ s_MemberPoolHead ].m_Next;
      s_MemberPool[ *inout_head ].m_Next = kMemberIdxTail;
      return &s_MemberPool[ *inout_head ];
    }
    else
    {
      _gjMember* member = &s_MemberPool[ *inout_head ];
      while ( member->m_Next != kMemberIdxTail )
      {
        member = &s_MemberPool[ member->m_Next ];
      }
      uint32_t new_idx = s_MemberPoolHead;
      s_MemberPoolHead = s_MemberPool[ s_MemberPoolHead ].m_Next;
      
      member->m_Next = new_idx;
      s_MemberPool[ new_idx ].m_Next = kMemberIdxTail;

      return &s_MemberPool[ new_idx ];
    }
  }

  return nullptr;
}

//---------------------------------------------------------------------------------
_gjMember* gj_freeMember( _gjMemberHandle* inout_head_handle, uint32_t key_crc32 )
{
  if ( inout_head_handle->m_Idx < s_Config.max_value_count )
  {
    _gjMember* member = &s_MemberPool[ inout_head_handle->m_Idx ];
    if ( member->m_Gen == inout_head_handle->m_Gen )
    {
      if ( member->m_KeyHash == key_crc32 )
      {
        const uint32_t prev_head = s_MemberPoolHead;
        s_MemberPoolHead = inout_head_handle->m_Idx;

        inout_head_handle->m_Idx = member->m_Next;
        inout_head_handle->m_Gen = inout_head_handle->m_Idx != kMemberIdxTail
                                 ? s_MemberPool[ inout_head_handle->m_Idx ].m_Gen
                                 : (uint32_t)-1;
        member->m_Gen++;
        member->m_Next = prev_head;
        gj_free( member->m_KeyStr );

        return member;
      }
      else
      {
        _gjMember* prev_member = member;
        while ( member->m_KeyHash != key_crc32 && member->m_Next != kMemberIdxTail )
        {
          prev_member = member;
          member = &s_MemberPool[ member->m_Next ];
        }

        if ( member->m_KeyHash == key_crc32 )
        {
          const uint32_t free_idx = prev_member->m_Next;
          prev_member->m_Next = member->m_Next;

          s_MemberPool[ free_idx ].m_Next = s_MemberPoolHead;
          s_MemberPool[ free_idx ].m_Gen++;
          s_MemberPoolHead = free_idx;
          gj_free( member->m_KeyStr );

          return &s_MemberPool[ free_idx ];
        }
        else
        {
          gj_assert( "Attempted to free object member that does not exist in provided head list. This is a problem with the library and should be reported to the github with a minimal repro." );
        }
      }
    }
    else
    {
      gj_assert( "Attempting to free a member that has already been freed" );
    }
  }
  else
  {
    gj_assert( "Attempting to free a member that has an invalid handle" );
  }

  return nullptr;
}

//---------------------------------------------------------------------------------
void gj_freeMemberList( _gjMemberHandle head_handle )
{
  if ( head_handle.m_Idx < s_Config.max_value_count )
  {
    _gjMember* member = &s_MemberPool[ head_handle.m_Idx ];
    if ( member->m_Gen == head_handle.m_Gen )
    {
      member->m_Gen++;
      while ( member->m_Next != kMemberIdxTail )
      {
        member = &s_MemberPool[ member->m_Next ];
        member->m_Gen++;
      }
      member->m_Next   = s_MemberPoolHead;
      s_MemberPoolHead = head_handle.m_Idx; 
    }
  }
}

//---------------------------------------------------------------------------------
bool gj_isValueAlloced( uint32_t idx, uint32_t gen )
{
  if ( idx < s_Config.max_value_count )
  {
    const uint32_t set_idx = idx >> 0x6;
    const uint64_t bit = ( 0x8000000000000000 >> ( idx & 0x3f ) );
    if ( s_ValueBitset[ set_idx ] & bit )
    {
      return s_ValuePool[ idx ].m_Gen == gen;
    }
  }
  return false;
}

//---------------------------------------------------------------------------------
void gj_freeValueData( _gjValue* val )
{
  if ( VAL_TYPE( val ) == gjValueType::kString )
  {
    gj_free( val->m_Str );
  }
  else if ( VAL_TYPE( val ) == gjValueType::kArray )
  {
    _gjArrayHandle arr_start_handle = val->m_ArrayStart;
    if ( arr_start_handle.m_Idx == kArrayIdxTail )
    {
      return;
    }

    _gjArrayElem*  elem = &s_ArrayPool[ arr_start_handle.m_Idx ];

    if ( elem->m_Gen == arr_start_handle.m_Gen )
    {
      if ( gj_isValueAlloced( elem->m_Value.idx, elem->m_Value.gen ) )
      {
        gj_freeValueData( &s_ValuePool[ elem->m_Value.idx ] );
        gj_freeValue    ( elem->m_Value.idx );
      }
      
      while ( elem->m_Next != kArrayIdxTail )
      {
        elem = &s_ArrayPool[ elem->m_Next ];
        if ( gj_isValueAlloced( elem->m_Value.idx, elem->m_Value.gen ) )
        {
          gj_freeValueData( &s_ValuePool[ elem->m_Value.idx ] );
          gj_freeValue    ( elem->m_Value.idx );
        }
      }

      gj_freeArrayElemList( arr_start_handle );
    }
  }
  else if ( VAL_TYPE( val ) == gjValueType::kObject )
  {
    _gjMemberHandle obj_start_handle = val->m_ObjectStart;
    if ( obj_start_handle.m_Idx == kMemberIdxTail )
    {
      return;
    }

    _gjMember*      member = &s_MemberPool[ obj_start_handle.m_Idx ];

    if ( member->m_Gen == obj_start_handle.m_Gen )
    {
      if ( gj_isValueAlloced( member->m_Value.idx, member->m_Value.gen ) )
      {
        gj_freeValueData( &s_ValuePool[ member->m_Value.idx ] );
        gj_freeValue    ( member->m_Value.idx );
        gj_free( member->m_KeyStr );
      }

      while ( member->m_Next != kMemberIdxTail )
      {
        member = &s_MemberPool[ member->m_Next ];
        if ( gj_isValueAlloced( member->m_Value.idx, member->m_Value.gen ) )
        {
          gj_freeValueData( &s_ValuePool[ member->m_Value.idx ] );
          gj_freeValue    ( member->m_Value.idx );
          gj_free( member->m_KeyStr );
        }
      }
    }

    gj_freeMemberList( obj_start_handle );
  }
}

//---------------------------------------------------------------------------------
//
// gjValue constructors
// 
//---------------------------------------------------------------------------------
gjValue::gjValue()
{
  gen = (uint32_t)-1;
  idx = (uint32_t)-1;
}

//---------------------------------------------------------------------------------
gjValue::gjValue( int v )
{
  gen = (uint32_t)-1;
  if ( _gjValue* val = gj_allocValue( &idx ) )
  {
    gen = val->m_Gen;
    ASSIGN_VAL_TYPE( val, gjValueType::kNumber );
    ASSIGN_VAL_SUBTYPE( val, kGjSubValueTypeInt );
    val->m_Int     = v;
  }
}

//---------------------------------------------------------------------------------
gjValue::gjValue( uint64_t v )
{
  gen = (uint32_t)-1;
  if ( _gjValue* val = gj_allocValue( &idx ) )
  {
    gen = val->m_Gen;
    ASSIGN_VAL_TYPE( val, gjValueType::kNumber );
    ASSIGN_VAL_SUBTYPE( val, kGjSubValueTypeU64 );
    val->m_U64     = v;
  }
}

//---------------------------------------------------------------------------------
gjValue::gjValue( float v )
{
  gen = (uint32_t)-1;
  if ( _gjValue* val = gj_allocValue( &idx ) )
  {
    gen = val->m_Gen;
    ASSIGN_VAL_TYPE( val, gjValueType::kNumber );
    ASSIGN_VAL_SUBTYPE( val, kGjSubValueTypeFloat );
    val->m_Float   = v;
  }
}

//---------------------------------------------------------------------------------
gjValue::gjValue( bool v )
{
  gen = (uint32_t)-1;
  if ( _gjValue* val = gj_allocValue( &idx ) )
  {
    gen = val->m_Gen;
    ASSIGN_VAL_TYPE( val, gjValueType::kBool );
    ASSIGN_VAL_SUBTYPE( val, kGjSubValueTypeInvalid );
    val->m_Bool    = v;
  }
}

//---------------------------------------------------------------------------------
gjValue::gjValue( const char* v )
{
  gen = (uint32_t)-1;
  if ( _gjValue* val = gj_allocValue( &idx ) )
  {
    gen = val->m_Gen;
    ASSIGN_VAL_TYPE( val, gjValueType::kString );
    ASSIGN_VAL_SUBTYPE( val, kGjSubValueTypeInvalid );

    size_t str_len = gj_StrLen( v );
    val->m_Str = (char*)gj_malloc( str_len + 1, "Value String" );
    if ( val->m_Str != nullptr )
    {
      memcpy( val->m_Str, v, str_len );
      val->m_Str[ str_len ] = '\0';
    }
    else
    {
      val->m_Gen++;
      gj_freeValue( idx );
    }
  }
}

//---------------------------------------------------------------------------------
//
// gjValue getters
// 
//---------------------------------------------------------------------------------
gjValueType gjValue::getType() const
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    return VAL_TYPE( val );
  }

  gj_assert( "Attempting to get type for deallocated json value" );
 
  return gjValueType::kInvalid;
}

//---------------------------------------------------------------------------------
int gjValue::getInt() const
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    if ( VAL_TYPE( val ) == gjValueType::kNumber)
    {
      switch ( VAL_SUBTYPE( val ) )
      {
      case kGjSubValueTypeInt:
      {
        return val->m_Int;
      }
      break;
      case kGjSubValueTypeU64:
      {
        return (int)val->m_U64;
      }
      break;
      case kGjSubValueTypeFloat:
      {
        return (int)val->m_Float;
      }
      break;
      }
    }
    else
    {
      gj_assert( "Attempting to get int for json value that is not a number" );
    }
  }
  else
  {
    gj_assert( "Attempting to get int for deallocated json value" );
  }

  return 0;
}

//---------------------------------------------------------------------------------
uint64_t gjValue::getU64() const
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    if ( VAL_TYPE( val ) == gjValueType::kNumber )
    {
      switch ( VAL_SUBTYPE( val ) )
      {
      case kGjSubValueTypeInt:
      {
        return (uint64_t)val->m_Int;
      }
      break;
      case kGjSubValueTypeU64:
      {
        return val->m_U64;
      }
      break;
      case kGjSubValueTypeFloat:
      {
        return (uint64_t)val->m_Float;
      }
      break;
      }
    }
    else
    {
      gj_assert( "Attempting to get u64 for json value that is not a number" );
    }
  }
  else
  {
    gj_assert( "Attempting to get u64 for deallocated json value" );
  }

  return 0;
}

//---------------------------------------------------------------------------------
float gjValue::getFloat() const
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    if ( VAL_TYPE( val ) == gjValueType::kNumber )
    {
      switch ( VAL_SUBTYPE( val ) )
      {
      case kGjSubValueTypeInt:
      {
        return (float)val->m_Int;
      }
      break;
      case kGjSubValueTypeU64:
      {
        return (float)val->m_U64;
      }
      break;
      case kGjSubValueTypeFloat:
      {
        return val->m_Float;
      }
      break;
      }
        
    }
    else
    {
      gj_assert( "Attempting to get float for json value that is not a number" );
    }
  }
  else
  {
    gj_assert( "Attempting to get float for deallocated json value" );
  }

  return 0;
}

//---------------------------------------------------------------------------------
const char* gjValue::getString() const
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    if ( VAL_TYPE( val ) == gjValueType::kString )
    {
      return val->m_Str;
    }
    else
    {
      gj_assert( "Attempting to get string for json value that is not a string" );
    }
  }
  else
  {
    gj_assert( "Attempting to get string for deallocated json value" );
  }

  return nullptr;
}

//---------------------------------------------------------------------------------
bool gjValue::getBool() const
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    if ( VAL_TYPE( val ) == gjValueType::kBool )
    {
      return val->m_Bool;
    }
    else
    {
      gj_assert( "Attempting to get bool for json value that is not a bool" );
    }
  }
  else
  {
    gj_assert( "Attempting to get bool for deallocated json value" );
  }

  return false;
}

//---------------------------------------------------------------------------------
//
// gjValue setters
// 
//---------------------------------------------------------------------------------
void gjValue::setInt( int v )
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    gj_freeValueData( val );

    ASSIGN_VAL_TYPE( val, gjValueType::kNumber );
    ASSIGN_VAL_SUBTYPE( val, kGjSubValueTypeInt );
    val->m_Int     = v;
  }
}

//---------------------------------------------------------------------------------
void gjValue::setU64( uint64_t v )
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    gj_freeValueData( val );

    ASSIGN_VAL_TYPE( val, gjValueType::kNumber );
    ASSIGN_VAL_SUBTYPE( val, kGjSubValueTypeU64 );
    val->m_U64     = v;
  }
}

//---------------------------------------------------------------------------------
void gjValue::setFloat( float v )
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    gj_freeValueData( val );

    ASSIGN_VAL_TYPE( val, gjValueType::kNumber );
    ASSIGN_VAL_SUBTYPE( val, kGjSubValueTypeFloat );
    val->m_Float   = v;
  }
}

//---------------------------------------------------------------------------------
void gjValue::setString( const char* str )
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    gj_freeValueData( val );

    ASSIGN_VAL_TYPE( val, gjValueType::kString );
    ASSIGN_VAL_SUBTYPE( val, kGjSubValueTypeInvalid );

    const size_t str_len = gj_StrLen( str );
    val->m_Str     = (char*)gj_malloc( str_len + 1, "setString string" );
    memcpy( val->m_Str, str, str_len );
    val->m_Str[ str_len ] = '\0';
  }
}

//---------------------------------------------------------------------------------
void gjValue::setBool( bool v )
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    gj_freeValueData( val );

    ASSIGN_VAL_TYPE( val, gjValueType::kBool );
    ASSIGN_VAL_SUBTYPE( val, kGjSubValueTypeInvalid );
    val->m_Float   = v;
  }
}

//---------------------------------------------------------------------------------
void gjValue::setNull()
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    gj_freeValueData( val );

    ASSIGN_VAL_TYPE( val, gjValueType::kNull );
    ASSIGN_VAL_SUBTYPE( val, kGjSubValueTypeInvalid );
  }
}

//---------------------------------------------------------------------------------
//
// gjValue assignment operators
// 
//---------------------------------------------------------------------------------
gjValue& gjValue::operator=( int v )
{
  setInt( v );
  return *this;
}

//---------------------------------------------------------------------------------
gjValue& gjValue::operator=( uint64_t v )
{
  setU64( v );
  return *this;
}

//---------------------------------------------------------------------------------
gjValue& gjValue::operator=( float v )
{
  setFloat( v );
  return *this;
}

//---------------------------------------------------------------------------------
gjValue& gjValue::operator=( const char* v )
{
  setString( v );
  return *this;
}

//---------------------------------------------------------------------------------
gjValue& gjValue::operator=( bool v )
{
  setBool( v );
  return *this;
}

//---------------------------------------------------------------------------------
//
// Make Deep Copy
//
//---------------------------------------------------------------------------------
gjValue gjValue::makeDeepCopy() const
{
  gjValue copy_val;
  if ( gj_isValueAlloced( idx, gen ) )
  {
    const _gjValue* val = &s_ValuePool[ idx ];
    if ( _gjValue* val_copy = gj_allocValue( &copy_val.idx ) )
    {
      copy_val.gen = val_copy->m_Gen;

      val_copy->m_TypeGroup = val->m_TypeGroup;
      switch ( VAL_TYPE( val ) )
      {
      case gjValueType::kBool:
      {
        val_copy->m_Bool = val->m_Bool;
      }
      break;
      case gjValueType::kString:
      {
        size_t sz = gj_StrLen( val->m_Str ) + 1;
        val_copy->m_Str = (char*)gj_malloc( sz, "string value" );
        memcpy( val_copy->m_Str, val->m_Str, sz );
      }
      break;
      case gjValueType::kNumber:
      {
        switch ( VAL_SUBTYPE( val ) )
        {
          case kGjSubValueTypeInt:
          {
            val_copy->m_Int = val->m_Int;
          }
          break;
          case kGjSubValueTypeU64:
          {
            val_copy->m_U64 = val->m_U64;
          }
          break;
          case kGjSubValueTypeFloat:
          {
            val_copy->m_Float = val->m_Float;
          }
          break;
        }
      }
      break;
      case gjValueType::kArray:
      {
        val_copy->m_ArrayStart.m_Idx = kArrayIdxTail;
        const _gjArrayHandle arr_handle_start = val->m_ArrayStart;
        if ( arr_handle_start.m_Gen == s_ArrayPool[ arr_handle_start.m_Idx ].m_Gen )
        {
          if ( arr_handle_start.m_Idx != kArrayIdxTail )
          {
            const _gjArrayElem* elem = &s_ArrayPool[ arr_handle_start.m_Idx ];

            _gjArrayElem* new_elem = gj_allocArrayElem( &val_copy->m_ArrayStart.m_Idx );
            val_copy->m_ArrayStart.m_Gen = new_elem->m_Gen;

            new_elem->m_Value = elem->m_Value.makeDeepCopy();

            while ( elem->m_Next != kArrayIdxTail )
            {
              elem = &s_ArrayPool[ elem->m_Next ];
              new_elem = gj_allocArrayElem( &val_copy->m_ArrayStart.m_Idx );
              new_elem->m_Value = elem->m_Value.makeDeepCopy();
            }
          }
        }
        break;
        case gjValueType::kObject:
        {
          val_copy->m_ObjectStart.m_Idx = kMemberIdxTail;
          val_copy->m_ObjectStart.m_Gen = (uint32_t)-1;

          if ( val->m_ObjectStart.m_Idx != kMemberIdxTail )
          {
            const _gjMemberHandle member_handle_start = val->m_ObjectStart;
            if ( member_handle_start.m_Gen == s_MemberPool[ member_handle_start.m_Idx ].m_Gen )
            {
              if ( member_handle_start.m_Idx != kMemberIdxTail )
              {
                const _gjMember* member = &s_MemberPool[ member_handle_start.m_Idx ];

                _gjMember* new_member   = gj_allocMember( &val_copy->m_ObjectStart.m_Idx );
                val_copy->m_ObjectStart.m_Gen = new_member->m_Gen;

                {
                  new_member->m_KeyHash = member->m_KeyHash;
                  new_member->m_Value   = member->m_Value.makeDeepCopy();
                  const size_t str_len  = gj_StrLen( member->m_KeyStr ) + 1;
                  new_member->m_KeyStr  = (char*)gj_malloc( str_len, "Object Member Key" );
                  memcpy( new_member->m_KeyStr, member->m_KeyStr, str_len );
                }

                while ( member->m_Next != kMemberIdxTail )
                {
                  member = &s_MemberPool[ member->m_Next ];

                  new_member            = gj_allocMember( &val_copy->m_ObjectStart.m_Idx );

                  new_member->m_KeyHash = member->m_KeyHash;
                  new_member->m_Value   = member->m_Value.makeDeepCopy();
                  const size_t str_len  = gj_StrLen( member->m_KeyStr ) + 1;
                  new_member->m_KeyStr  = (char*)gj_malloc( str_len, "Object Member Key" );
                  memcpy( new_member->m_KeyStr, member->m_KeyStr, str_len );
                }
              }
            }
          }
        }
        break;
      }
      break;
      }
    }
    else
    {
      gj_assert( "backing data is not large enough to make deep copy" );
    }
  }

  return copy_val;
}

//---------------------------------------------------------------------------------
//
// Array operations
//
//---------------------------------------------------------------------------------
uint32_t gjValue::getElementCount() const
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    const _gjValue* val = &s_ValuePool[ idx ];
    if ( VAL_TYPE( val ) == gjValueType::kArray )
    {
      const _gjArrayHandle arr_handle_start = val->m_ArrayStart;
      if ( arr_handle_start.m_Idx == kArrayIdxTail )
      {
        return 0;
      }
      if ( arr_handle_start.m_Gen == s_ArrayPool[ arr_handle_start.m_Idx ].m_Gen )
      {
        uint32_t count = 0;

        uint32_t elem_idx = arr_handle_start.m_Idx;
        while ( elem_idx != kArrayIdxTail )
        {
          elem_idx = s_ArrayPool[ elem_idx ].m_Next;
          count++;
        }

        return count;
      }
      else
      {
        gj_assert( "Attempting to get element count for an array that has been freed" );
      }

    }
    else
    {
      gj_assert( "Attempting to get element count for json value that is not a array" );
    }
  }
  else
  {
    gj_assert( "Attempting to get element count for deallocated json value" );
  }

  return 0;
}

//---------------------------------------------------------------------------------
gjValue gjValue::getElement( uint32_t elem_idx ) const
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    const _gjValue* val = &s_ValuePool[ idx ];
    if ( VAL_TYPE( val ) == gjValueType::kArray )
    {
      const _gjArrayHandle arr_handle_start = val->m_ArrayStart;
      if ( arr_handle_start.m_Gen == s_ArrayPool[ arr_handle_start.m_Idx ].m_Gen )
      {
        if ( arr_handle_start.m_Idx != kArrayIdxTail )
        {
          uint32_t cur_idx = 0;
          const _gjArrayElem* elem = &s_ArrayPool[ arr_handle_start.m_Idx ];
          while ( cur_idx != elem_idx && elem->m_Next != kArrayIdxTail )
          {
            cur_idx++;
            elem = &s_ArrayPool[ elem->m_Next ];
          }
          return cur_idx != elem_idx ? gjValue() : elem->m_Value;
        }
        return gjValue();
      }
      else
      {
        gj_assert( "Attempting to get element for an array that has been freed" );
      }

    }
    else
    {
      gj_assert( "Attempting to get element for json value that is not a array" );
    }
  }
  else
  {
    gj_assert( "Attempting to get element for deallocated json value" );
  }

  return gjValue{};
}

//---------------------------------------------------------------------------------
gjValue gjValue::operator[]( uint32_t elem_idx ) const
{
  return getElement( elem_idx );
}

//---------------------------------------------------------------------------------
void gjValue::insertElement( gjValue value, uint32_t insert_idx )
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    if ( VAL_TYPE( val ) == gjValueType::kArray )
    {
      uint32_t arr_head_idx = val->m_ArrayStart.m_Idx;
      _gjArrayElem* elem = gj_allocArrayElem( &arr_head_idx, insert_idx );

      if ( elem != nullptr )
      {
        if ( arr_head_idx != val->m_ArrayStart.m_Idx )
        {
          val->m_ArrayStart.m_Idx = arr_head_idx;
          val->m_ArrayStart.m_Gen = elem->m_Gen;
        }

        elem->m_Value = value;
      }
      else
      {
        gj_assert( "Attempting to create array element, but the array pool is full. You may be out of memory" );
      }
    }
    else
    {
      gj_assert( "Attempting to insert element into a value that isn't an array" );
    }
  }
  else
  {
    gj_assert( "Attempting to insert element into a value that has been freed" );
  }
}

//---------------------------------------------------------------------------------
void gjValue::removeElement( uint32_t remove_idx )
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    if ( VAL_TYPE( val ) == gjValueType::kArray)
    {
      const _gjArrayElem* freed_elem  = gj_freeArrayElem( &val->m_ArrayStart, remove_idx );
      if ( freed_elem )
      {
        _gjValue* freed_value = &s_ValuePool[ freed_elem->m_Value.idx ];
        gj_freeValueData( freed_value );
        gj_freeValue    ( freed_elem->m_Value.idx );
      }
    }
    else
    {
      gj_assert( "Attempting to remove element into a value that isn't an array" );
    }
  }
  else
  {
    gj_assert( "Attempting to remove element from a value that has been freed" );
  }
}

//---------------------------------------------------------------------------------
void gjValue::clearArray()
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    if ( VAL_TYPE( val ) == gjValueType::kArray)
    {
      gj_freeValueData( val );
      val->m_ArrayStart.m_Idx = (uint32_t)-1;
      val->m_ArrayStart.m_Gen = (uint32_t)-1;
    }
    else
    {
      gj_assert( "Attempting to clear array on a value that isn't an array" );
    }
  }
  else
  {
    gj_assert( "Attempting to clear array on a value that has been freed" );
  }
}

//---------------------------------------------------------------------------------
//
// Object operations
//
//---------------------------------------------------------------------------------
uint32_t gjValue::getMemberCount() const
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    const _gjValue* val = &s_ValuePool[ idx ];
    if ( VAL_TYPE( val ) == gjValueType::kObject )
    {
      const _gjMemberHandle obj_handle_start = val->m_ObjectStart;
      if ( obj_handle_start.m_Idx == kMemberIdxTail )
      {
        return 0;
      }

      if ( obj_handle_start.m_Gen == s_MemberPool[ obj_handle_start.m_Idx ].m_Gen )
      {
        uint32_t count = 0;

        uint32_t member_idx = obj_handle_start.m_Idx;
        while ( member_idx != kMemberIdxTail )
        {
          member_idx = s_MemberPool[ member_idx ].m_Next;
          count++;
        }

        return count;
      }
      else
      {
        gj_assert( "Attempting to get member count for an object that has been freed" );
      }

    }
    else
    {
      gj_assert( "Attempting to get member count for json value that is not an object" );
    }
  }
  else
  {
    gj_assert( "Attempting to get member count for deallocated json value" );
  }

  return 0;
}

//---------------------------------------------------------------------------------
gjValue gjValue::operator[]( const char* key ) const
{
  return getMember( key );
}

//---------------------------------------------------------------------------------
gjValue gjValue::getMember( const char* key ) const
{
  return getMember( gj_crc32( key ) );
}

//---------------------------------------------------------------------------------
gjValue gjValue::getMember( uint32_t key_crc32 ) const
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    const _gjValue* val = &s_ValuePool[ idx ];
    if ( VAL_TYPE( val ) == gjValueType::kObject )
    {
      const _gjMemberHandle member_handle_start = val->m_ObjectStart;
      if ( member_handle_start.m_Idx != kMemberIdxTail )
      {
        if ( member_handle_start.m_Gen == s_MemberPool[ member_handle_start.m_Idx ].m_Gen )
        {
          uint32_t cur_idx = member_handle_start.m_Idx;
          const _gjMember* member = &s_MemberPool[ cur_idx ];
          while ( member->m_KeyHash != key_crc32 && member->m_Next != kMemberIdxTail )
          {
            cur_idx = member->m_Next;
            member = &s_MemberPool[ cur_idx ];
          }

          if ( member->m_KeyHash == key_crc32 )
          {
            return member->m_Value;
          }
          else
          {
            gj_assert( "Attempting to get member that does not exist in object" );
          }
        }
        else
        {
          gj_assert( "Attempting to get member for an object that has been freed" );
        }
      }
      else
      {
        gj_assert( "Attempting to get member in an empty object" );
      }
    }
    else
    {
      gj_assert( "Attempting to get member from a value that isn't an object" );
    }
  }
  else
  {
    gj_assert( "Attempting to get member from a value that has been freed" );
  }

  return gjValue();
}

//---------------------------------------------------------------------------------
bool gjValue::hasMember( const char* key ) const
{
  return hasMember( gj_crc32( key ) );
}

//---------------------------------------------------------------------------------
bool gjValue::hasMember( uint32_t key_crc32 ) const
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    const _gjValue* val = &s_ValuePool[ idx ];
    if ( VAL_TYPE( val ) == gjValueType::kObject )
    {
      const _gjMemberHandle member_handle_start = val->m_ObjectStart;
      if ( member_handle_start.m_Idx == kMemberIdxTail )
      {
        return false;
      }

      if ( member_handle_start.m_Gen == s_MemberPool[ member_handle_start.m_Idx ].m_Gen )
      {
        uint32_t cur_idx = member_handle_start.m_Idx;
        const _gjMember* member = &s_MemberPool[ cur_idx ];
        while ( member->m_KeyHash != key_crc32 && member->m_Next != kMemberIdxTail )
        {
          cur_idx = member->m_Next;
          member = &s_MemberPool[ cur_idx ];
        }

        return member->m_KeyHash == key_crc32;
      }
      else
      {
        gj_assert( "Attempting to get member for an object that has been freed" );
      }
    }
    else
    {
      gj_assert( "Attempting to get member from a value that isn't an object" );
    }
  }
  else
  {
    gj_assert( "Attempting to get member from a value that has been freed" );
  }

  return false;
}

//---------------------------------------------------------------------------------
void gjValue::addMember( const char* key, gjValue value )
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    if ( VAL_TYPE( val ) == gjValueType::kObject )
    {
      uint32_t head_idx = val->m_ObjectStart.m_Idx;
      _gjMember* member = gj_allocMember( &head_idx );

      if ( member != nullptr )
      {
        if ( head_idx != val->m_ObjectStart.m_Idx )
        {
          val->m_ObjectStart.m_Idx = head_idx;
          val->m_ObjectStart.m_Gen = member->m_Gen;
        }

        const size_t key_str_size = gj_StrLen( key );
        member->m_KeyStr = (char*)gj_malloc( key_str_size + 1, "Object Key String" );
        memcpy( member->m_KeyStr, key, key_str_size );
        member->m_KeyStr[ key_str_size ] = '\0';
        member->m_KeyHash = gj_crc32( key );
        member->m_Value   = value;
      }
      else
      {
        gj_assert( "Attempting to create object member, but the  member pool is full. You may be out of memory" );
      }
    }
    else
    {
      gj_assert( "Attempting to add member to a value that isn't an object" );
    }
  }
  else
  {
    gj_assert( "Attempting to add member to a value that has been freed" );
  }
}

//---------------------------------------------------------------------------------
void gjValue::removeMember( const char* key )
{
  removeMember( gj_crc32( key ) );
}

//---------------------------------------------------------------------------------
void gjValue::removeMember( uint32_t key_crc32 )
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    if ( VAL_TYPE( val ) == gjValueType::kObject )
    {
      const _gjMember* freed_elem  = gj_freeMember( &val->m_ObjectStart, key_crc32 );
      if ( freed_elem != nullptr )
      {
        _gjValue* freed_value = &s_ValuePool[ freed_elem->m_Value.idx ];
        gj_freeValueData( freed_value );
        gj_freeValue    ( freed_elem->m_Value.idx );
      }
    }
    else
    {
      gj_assert( "Attempting to remove member from a value that isn't an object" );
    }
  }
  else
  {
    gj_assert( "Attempting to remove member from a value that has been freed" );
  }
}

//---------------------------------------------------------------------------------
void gjValue::clearObject()
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    if ( VAL_TYPE( val ) == gjValueType::kObject)
    {
      gj_freeValueData( val );
      val->m_ObjectStart.m_Idx = (uint32_t)-1;
      val->m_ObjectStart.m_Gen = (uint32_t)-1;
    }
    else
    {
      gj_assert( "Attempting to clear array on a value that isn't an array" );
    }
  }
  else
  {
    gj_assert( "Attempting to clear array on a value that has been freed" );
  }
}

//---------------------------------------------------------------------------------
gjMembers gjValue::members()
{
  gjMembers members;
  members.value = *this;
  return members;
}

//---------------------------------------------------------------------------------
gjMembers::iterator gjMembers::begin()
{
  iterator it;
  it.idx = kMemberIdxTail;
  it.gen = (uint32_t)-1;
  if ( gj_isValueAlloced( value.idx, value.gen ) )
  {
    it.idx = s_ValuePool[ value.idx ].m_ObjectStart.m_Idx;
    it.gen = s_ValuePool[ value.idx ].m_ObjectStart.m_Gen;
  }
  return it;
}

//---------------------------------------------------------------------------------
gjMembers::iterator gjMembers::end()
{
  iterator it;
  it.idx = kMemberIdxTail;
  it.gen = (uint32_t)-1;
  return it;
}

//---------------------------------------------------------------------------------
gjObjectMember gjMemberIterator::operator*()
{
  if ( idx < s_Config.max_value_count && s_MemberPool[ idx ].m_Gen == gen )
  {
    _gjMember* member = &s_MemberPool[ idx ];
    return { member->m_KeyStr, member->m_Value };
  }

  return { nullptr, gjValue() };
}

//---------------------------------------------------------------------------------
bool gjMemberIterator::operator==( const gjMemberIterator& other ) const
{
  return idx == other.idx && gen == other.gen;
}

//---------------------------------------------------------------------------------
gjMemberIterator& gjMemberIterator::operator++()
{
  if ( idx < s_Config.max_value_count && s_MemberPool[ idx ].m_Gen == gen && s_MemberPool[ idx ].m_Next != kMemberIdxTail )
  {
    _gjMember* next = &s_MemberPool[ s_MemberPool[ idx ].m_Next ];
    idx = s_MemberPool[ idx ].m_Next;
    gen = next->m_Gen;
  }
  else
  {
    idx = kMemberIdxTail;
    gen = (uint32_t)-1;
  }

  return *this;
}



//---------------------------------------------------------------------------------
//
// General controls
// 
//---------------------------------------------------------------------------------
gjValue gj_makeArray()
{
  gjValue handle_val{};
  if ( _gjValue* val = gj_allocValue( &handle_val.idx ) )
  {
    handle_val.gen = val->m_Gen;
    ASSIGN_VAL_TYPE( val, gjValueType::kArray );
    ASSIGN_VAL_SUBTYPE( val, kGjSubValueTypeInvalid );
    val->m_ArrayStart.m_Gen = (uint32_t)-1;
    val->m_ArrayStart.m_Idx = (uint32_t)-1;
  }
  return handle_val;
}

//---------------------------------------------------------------------------------
gjValue gj_makeObject()
{
  gjValue handle_val{};
  if ( _gjValue* val = gj_allocValue( &handle_val.idx ) )
  {
    handle_val.gen = val->m_Gen;
    ASSIGN_VAL_TYPE( val, gjValueType::kObject );
    ASSIGN_VAL_SUBTYPE( val, kGjSubValueTypeInvalid );
    val->m_ObjectStart.m_Gen = (uint32_t)-1;
    val->m_ObjectStart.m_Idx = (uint32_t)-1;
  }
  return handle_val;
}

//---------------------------------------------------------------------------------
void gj_deleteValue( gjValue val )
{
  if ( val.idx < s_Config.max_value_count && val.gen == s_ValuePool[val.idx].m_Gen )
  {
    _gjValue* internal_val = &s_ValuePool[ val.idx ];
    gj_freeValueData( internal_val );
    gj_freeValue( val.idx );
  }
}

//---------------------------------------------------------------------------------
uint32_t gj_getArrayElemFreeCount()
{
  uint32_t count = 0;
  uint32_t idx = s_ArrayPoolHead;
  while ( idx != kArrayIdxTail )
  {
    count++;
    idx = s_ArrayPool[ idx ].m_Next;
  }

  return count;
}

//---------------------------------------------------------------------------------
uint32_t gj_getObjectMemberFreeCount()
{
  uint32_t count = 0;
  uint32_t idx = s_MemberPoolHead;
  while ( idx != kMemberIdxTail )
  {
    count++;
    idx = s_MemberPool[ idx ].m_Next;
  }

  return count;
}

//---------------------------------------------------------------------------------
uint32_t gj_getValueAllocedCount()
{
  uint32_t count = 0;
  for ( uint32_t i_word = 0; i_word < s_ValueBitsetWordCount; ++i_word )
  {
    count += (uint32_t)__popcnt64( s_ValueBitset[i_word] );
  }
  return count;
}

//---------------------------------------------------------------------------------
gjUsageStats gj_getUsageStats()
{
  gjUsageStats stats;
  stats.m_FreeArrayElements = gj_getArrayElemFreeCount();
  stats.m_UsedArrayElements = s_Config.max_value_count - stats.m_FreeArrayElements;
  stats.m_FreeObjectMembers = gj_getObjectMemberFreeCount();
  stats.m_UsedObjectMembers = s_Config.max_value_count - stats.m_FreeObjectMembers;
  stats.m_UsedValues        = gj_getValueAllocedCount();
  stats.m_FreeValues        = s_Config.max_value_count - stats.m_UsedValues;

  return stats;

}

//---------------------------------------------------------------------------------
gjSerializeOptions gj_getDefaultSerializeOptions()
{
  gjSerializeOptions options;
  options.mode          = gjSerializeMode::kPretty;
  options.newline_style = gjNewlineStyle::kLinux;
  options.indent_amt    = 2;

  return options;
}

//---------------------------------------------------------------------------------
gjSerializer::gjSerializer( gjValue obj_to_serialize, gjSerializeOptions* options )
: m_Obj( obj_to_serialize )
, m_Options( options ? *options : gj_getDefaultSerializeOptions() )
, m_StringData( nullptr )
{
}

//---------------------------------------------------------------------------------
gjSerializer::~gjSerializer()
{
  if ( m_StringData != nullptr )
  {
    gj_free( m_StringData );
  }
}

//---------------------------------------------------------------------------------
constexpr const char* kNewlineStrings[] = {
  "\r\n",
  "\n"
};
static_assert( sizeof( kNewlineStrings ) / sizeof( *kNewlineStrings ) == (size_t)gjNewlineStyle::kCount, "The must stay in sync" );


//---------------------------------------------------------------------------------
constexpr size_t kNewlineLens[] = {
  2,
  1
};
static_assert( sizeof( kNewlineLens ) / sizeof( *kNewlineLens ) == (size_t)gjNewlineStyle::kCount, "The must stay in sync" );

//---------------------------------------------------------------------------------
size_t gj_getJsonSizeforCString( const char* cstring )
{
  size_t sz = 0;
  for ( uint32_t i_char = 0; cstring[ i_char ] != '\0'; ++i_char, ++sz )
  {
    sz += cstring[i_char + 1] == '\r'
       || cstring[i_char + 1] == '\n'
       || cstring[i_char + 1] == '\t'
       || cstring[i_char + 1] == '\b'
       || cstring[i_char + 1] == '\f'
       || cstring[i_char + 1] == '\\'
       || cstring[i_char + 1] == '/'
       || cstring[i_char + 1] == '"';
  }

  return sz;
}

//---------------------------------------------------------------------------------
size_t gj_getRequiredSerializedSize( gjValue val_handle, gjSerializeOptions* options, size_t indent_amt = 0 )
{
  const size_t newline_len    = options->mode == gjSerializeMode::kPretty ? kNewlineLens[ (uint32_t)options->newline_style ] : 0;
  const size_t new_indent_amt = options->mode == gjSerializeMode::kPretty ? abs( options->indent_amt )                       : 0;

  if ( gj_isValueAlloced( val_handle.idx, val_handle.gen ) )
  {
    _gjValue* val = &s_ValuePool[ val_handle.idx ];
    switch ( VAL_TYPE( val ) )
    {
    case gjValueType::kNull:
    {
      return 4; // null
    }
    case gjValueType::kObject:
    {
      const size_t local_indent_amt = indent_amt + new_indent_amt;
      size_t sz = 1 + newline_len; // {

      if ( val->m_ObjectStart.m_Idx < s_Config.max_value_count )
      {
        _gjMember* member = &s_MemberPool[ val->m_ObjectStart.m_Idx ];
        if ( member->m_Gen == val->m_ObjectStart.m_Gen )
        {
          while ( member->m_Next != kMemberIdxTail )
          {
            sz += local_indent_amt + (options->mode == gjSerializeMode::kMinified ? 3 : 5) + gj_getJsonSizeforCString( member->m_KeyStr ); // "<key>" : 
            sz += gj_getRequiredSerializedSize( member->m_Value, options, local_indent_amt );
            sz += 1 + newline_len; // ,

            member = &s_MemberPool[ member->m_Next ];
          }
          sz += local_indent_amt + (options->mode == gjSerializeMode::kMinified ? 3 : 5) + gj_getJsonSizeforCString( member->m_KeyStr ); // "<key>" : 
          sz += gj_getRequiredSerializedSize( member->m_Value, options, local_indent_amt );
          sz += newline_len;
        }
        else
        {
          gj_assert( "attempting to serialize an object member that has been freed" );
        }
      }
      sz += indent_amt + 1; // }
      return sz;
    }
    case gjValueType::kArray:
    {
      size_t sz = 1; // [

      if ( val->m_ArrayStart.m_Idx == kArrayIdxTail )
      {
        sz += 1; // ]
        return sz;
      }

      sz += newline_len;

      _gjArrayElem* elem = &s_ArrayPool[ val->m_ArrayStart.m_Idx ];
      if ( elem->m_Gen == val->m_ArrayStart.m_Gen )
      {
        const size_t local_indent_amt = indent_amt + new_indent_amt;
        while ( elem->m_Next != kArrayIdxTail )
        {
          sz += local_indent_amt + newline_len + 1; // ,
          sz += gj_getRequiredSerializedSize( elem->m_Value, options, local_indent_amt );
          elem = &s_ArrayPool[ elem->m_Next ];
        }

        sz += local_indent_amt + newline_len;
        sz += gj_getRequiredSerializedSize( elem->m_Value, options, local_indent_amt );
      }
      else
      {
        gj_assert( "attempting to serialize an array element that has been freed" );
      }

      sz += indent_amt + newline_len + 1; // ]
      return sz;
    }
    case gjValueType::kString:
    {
      return 2 + gj_getJsonSizeforCString( val->m_Str ); // "<string>"
    }
    case gjValueType::kNumber:
    {
      switch ( VAL_SUBTYPE( val ) )
      {
        case kGjSubValueTypeInt:
        {
          return 11;
        }
        break;
        case kGjSubValueTypeU64:
        {
          return 20;
        }
        break;
        case kGjSubValueTypeFloat:
        {
          return 20; // todo: this is likely not correct
        }
        break;
      }
    }
    case gjValueType::kBool:
    {
      return val->m_Bool ? 4 : 5;
    }
    default:
      gj_assert( "Attempt to serialize unknown type!" );
    }
  }
  else
  {
    gj_assert( "attempting to serialize freed value" );
  }

  return 0;
}

//---------------------------------------------------------------------------------
char* gj_addIndent( char* cursor, size_t amt, bool use_tabs )
{
  for ( size_t i_char = 0; i_char < amt; ++i_char )
  {
    *cursor = use_tabs ? '\t' : ' ';
    cursor++;
  }
  return cursor;
}

//---------------------------------------------------------------------------------
char* gj_addChars( char* cursor, const char* src, size_t len )
{
  memcpy( cursor, src, len );
  return cursor + len;
}

//---------------------------------------------------------------------------------
char* gj_addCString( char* cursor, const char* src, size_t src_len )
{
  uint32_t offset = 0;
  for ( uint32_t i_char = 0; i_char < src_len; ++i_char )
  {
    switch( src[ i_char ] )
    {
    case '\\':
    case '/':
    case '"':
    {
      cursor[ i_char + offset++ ] = '\\';
      cursor[ i_char + offset   ] = src[ i_char ];
    }
    break;
    case '\n':
    {
      cursor[ i_char + offset++ ] = '\\';
      cursor[ i_char + offset   ] = 'n';
    }
    break;
    case '\r':
    {
      cursor[ i_char + offset++ ] = '\\';
      cursor[ i_char + offset   ] = 'r';
    }
    break;
    case '\b':
    {
      cursor[ i_char + offset++ ] = '\\';
      cursor[ i_char + offset   ] = 'b';
    }
    break;
    case '\f':
    {
      cursor[ i_char + offset++ ] = '\\';
      cursor[ i_char + offset   ] = 'f';
    }
    break;
    case '\t':
    {
      cursor[ i_char + offset++ ] = '\\';
      cursor[ i_char + offset   ] = 't';
    }
    break;
    default:
      cursor[ i_char + offset ] =  src[ i_char ];
    }
  }

  return cursor + src_len - offset;
}

//---------------------------------------------------------------------------------
char* gj_serialize( char* cursor, gjValue val_handle, gjSerializeOptions* options, size_t indent_amt = 0 )
{
  const char*  newline_str    = options->mode == gjSerializeMode::kPretty ? kNewlineStrings[ (uint32_t)options->newline_style ] : "";
  const size_t newline_len    = options->mode == gjSerializeMode::kPretty ? kNewlineLens   [ (uint32_t)options->newline_style ] : 0;
  const bool   using_tabs     = options->mode == gjSerializeMode::kPretty ? options->indent_amt == -1                           : 0;
  const size_t new_indent_amt = options->mode == gjSerializeMode::kPretty ? abs( options->indent_amt )                          : 0;

  if ( gj_isValueAlloced( val_handle.idx, val_handle.gen ) )
  {
    _gjValue* val = &s_ValuePool[ val_handle.idx ];
    switch ( VAL_TYPE( val ) )
    {
    case gjValueType::kNull:
    {
      return gj_addChars( cursor, "null", 4 );
    }
    case gjValueType::kObject:
    {
      const size_t local_indent_amt = indent_amt + new_indent_amt;
      cursor = gj_addChars( cursor, "{",          1           );
      cursor = gj_addChars( cursor,  newline_str, newline_len );

      if (val->m_ObjectStart.m_Idx < s_Config.max_value_count)
      {

        _gjMember* member = &s_MemberPool[ val->m_ObjectStart.m_Idx ];
        if ( member->m_Gen == val->m_ObjectStart.m_Gen )
        {
          while ( member->m_Next != kMemberIdxTail )
          {
            cursor = gj_addIndent ( cursor, local_indent_amt, using_tabs                    );
            cursor = gj_addChars  ( cursor, "\"",             1                             );
            cursor = gj_addCString( cursor, member->m_KeyStr, gj_StrLen( member->m_KeyStr ) );
            cursor = gj_addChars  ( cursor, options->mode == gjSerializeMode::kMinified ? "\":" : "\" : ", 
                                            options->mode == gjSerializeMode::kMinified ? 2     : 4 );

            cursor = gj_serialize ( cursor, member->m_Value, options, local_indent_amt );
            cursor = gj_addChars  ( cursor, ",",              1           );
            cursor = gj_addChars  ( cursor, newline_str,      newline_len );

            member = &s_MemberPool[ member->m_Next ];
          }

          cursor = gj_addIndent ( cursor, local_indent_amt, using_tabs                    );
          cursor = gj_addChars  ( cursor, "\"",             1                             );
          cursor = gj_addCString( cursor, member->m_KeyStr, gj_StrLen( member->m_KeyStr ) );
          cursor = gj_addChars  ( cursor, options->mode == gjSerializeMode::kMinified ? "\":" : "\" : ", 
                                          options->mode == gjSerializeMode::kMinified ? 2     : 4 );

          cursor = gj_serialize ( cursor, member->m_Value, options, local_indent_amt );
          cursor = gj_addChars  ( cursor, newline_str,      newline_len );
        }
        else
        {
          gj_assert( "attempting to serialize an object member that has been freed" );
        }

      }

      cursor = gj_addIndent( cursor, indent_amt,  using_tabs );
      cursor = gj_addChars ( cursor, "}",         1          );
      return cursor;
    }
    case gjValueType::kArray:
    {
      cursor = gj_addChars( cursor, "[", 1 );
      if ( val->m_ArrayStart.m_Idx == kArrayIdxTail )
      {
        cursor = gj_addChars( cursor, "]", 1 );
        return cursor;
      }

      cursor = gj_addChars( cursor, newline_str, newline_len );

      _gjArrayElem* elem = &s_ArrayPool[ val->m_ArrayStart.m_Idx ];
      if ( elem->m_Gen == val->m_ArrayStart.m_Gen )
      {
        const size_t local_indent_amt = indent_amt + new_indent_amt;
        while ( elem->m_Next != kArrayIdxTail )
        {
          cursor = gj_addIndent( cursor, local_indent_amt, using_tabs );
          cursor = gj_serialize( cursor, elem->m_Value, options, local_indent_amt );
          cursor = gj_addChars ( cursor, ",", 1 );
          cursor = gj_addChars ( cursor, newline_str, newline_len );

          elem = &s_ArrayPool[ elem->m_Next ];
        }

        cursor = gj_addIndent( cursor, local_indent_amt, using_tabs );
        cursor = gj_serialize( cursor, elem->m_Value, options, local_indent_amt );
        cursor = gj_addChars ( cursor, newline_str, newline_len );
      }
      else
      {
        gj_assert( "attempting to serialize an array element that has been freed" );
      }

      cursor = gj_addIndent( cursor, indent_amt, using_tabs );
      cursor = gj_addChars ( cursor, "]",        1          );
      return cursor;
    }
    case gjValueType::kString:
    {
      cursor = gj_addChars  ( cursor, "\"", 1 );
      cursor = gj_addCString( cursor, val->m_Str, gj_StrLen( val->m_Str ) );
      cursor = gj_addChars  ( cursor, "\"", 1 );
      return cursor;
    }
    case gjValueType::kNumber:
    {
      switch ( VAL_SUBTYPE( val ) )
      {
        case kGjSubValueTypeInt:
        {
          snprintf( cursor, 10, "%d", val->m_Int );
          return cursor + gj_StrLen( cursor );
        }
        break;
        case kGjSubValueTypeU64:
        {
          snprintf( cursor, 20, "%llu", val->m_U64 );
          return cursor + gj_StrLen( cursor );
        }
        break;
        case kGjSubValueTypeFloat:
        {
          snprintf( cursor, 20, "%0.18f", val->m_Float );
          return cursor + gj_StrLen( cursor );
        }
        break;
      }
    }
    case gjValueType::kBool:
    {
      if ( val->m_Bool )
      {
        return gj_addChars( cursor, "true", 4 );
      }
      else
      {
        return gj_addChars( cursor, "false", 5 );
      }
    }
    default:
      gj_assert( "Attempt to serialize unknown type!" );
    }
  }
  else
  {
    gj_assert( "attempting to serialize freed value" );
  }

  return nullptr;
}

//---------------------------------------------------------------------------------
void gjSerializer::serialize()
{
  // gather the size needed
  size_t str_sz = gj_getRequiredSerializedSize( m_Obj, &m_Options ) + 1;
  if ( m_StringData != nullptr )
  {
    gj_free( m_StringData );
  }

  m_StringData = (char*)gj_malloc( str_sz, "Serialized string data" );
  char* cursor = m_StringData;
  cursor = gj_serialize( cursor, m_Obj, &m_Options );
  *cursor = '\0';
}

//---------------------------------------------------------------------------------
const char* gjSerializer::getString()
{
  return m_StringData ? m_StringData : "";
}

//---------------------------------------------------------------------------------
enum _gjLexSymType : uint16_t
{
  kSymOpenBrace,
  kSymClosedBrace,
  kSymComma,
  kSymColon,
  kSymOpenBracket,
  kSymClosedBracket,
  kSymString,
  kSymFloat,
  kSymInt,
  kSymU64,
  kSymBool,
  kSymNull,

  kSymCount
};

//---------------------------------------------------------------------------------
struct _gjLexSym
{
  const char*    m_Str;
  uint16_t       m_StrLen;
  _gjLexSymType  m_Type;
};

//---------------------------------------------------------------------------------
struct _gjLexContext
{
  const char* m_Cursor;
  _gjLexSym*  m_Syms;
  size_t      m_MaxLen;
  uint32_t    m_SymCount;
  uint32_t    m_ReadIdx;
};

//---------------------------------------------------------------------------------
bool gj_lexWhitespace( _gjLexContext* ctx )
{
  if ( *ctx->m_Cursor == ' '  ||
       *ctx->m_Cursor == '\r' ||
       *ctx->m_Cursor == '\n' ||
       *ctx->m_Cursor == '\t' )
  {
    ctx->m_Cursor++;
    return true;
  }

  return false;
}

//---------------------------------------------------------------------------------
_gjLexSym* gj_newSym( _gjLexContext* ctx )
{
  if ( ctx->m_SymCount == ctx->m_MaxLen )
  {
    gj_assert( "ran out of space in lex syms. You may need to provide a larger max count in the initialization config for this data" );
  }
  return &ctx->m_Syms[ ctx->m_SymCount++ ];
}


//---------------------------------------------------------------------------------
bool gj_lexOpenBrace( _gjLexContext* ctx )
{
  if ( *ctx->m_Cursor == '{' )
  {
    _gjLexSym* sym = gj_newSym( ctx );
    sym->m_Str     = ctx->m_Cursor;
    sym->m_StrLen  = 1;
    sym->m_Type    = kSymOpenBrace;
    ctx->m_Cursor++;
    return true;
  }
  return false;
}

//---------------------------------------------------------------------------------
bool gj_lexClosedBrace( _gjLexContext* ctx )
{
  if ( *ctx->m_Cursor == '}' )
  {
    _gjLexSym* sym = gj_newSym( ctx );
    sym->m_Str     = ctx->m_Cursor;
    sym->m_StrLen  = 1;
    sym->m_Type    = kSymClosedBrace;
    ctx->m_Cursor++;
    return true;
  }
  return false;
}

//---------------------------------------------------------------------------------
bool gj_lexComma( _gjLexContext* ctx )
{
  if ( *ctx->m_Cursor == ',' )
  {
    _gjLexSym* sym   = gj_newSym( ctx );
    sym->m_Str    = ctx->m_Cursor;
    sym->m_StrLen = 1;
    sym->m_Type   = kSymComma;
    ctx->m_Cursor++;
    return true;
  }
  return false;
}

//---------------------------------------------------------------------------------
bool gj_lexColon( _gjLexContext* ctx )
{
  if ( *ctx->m_Cursor == ':' )
  {
    _gjLexSym* sym = gj_newSym( ctx );
    sym->m_Str     = ctx->m_Cursor;
    sym->m_StrLen  = 1;
    sym->m_Type    = kSymColon;
    ctx->m_Cursor++;
    return true;
  }
  return false;
}

//---------------------------------------------------------------------------------
bool gj_lexOpenBracket( _gjLexContext* ctx )
{
  if ( *ctx->m_Cursor == '[' )
  {
    _gjLexSym* sym = gj_newSym( ctx );
    sym->m_Str     = ctx->m_Cursor;
    sym->m_StrLen  = 1;
    sym->m_Type    = kSymOpenBracket;
    ctx->m_Cursor++;
    return true;
  }
  return false;
}

//---------------------------------------------------------------------------------
bool gj_lexClosedBracket( _gjLexContext* ctx )
{
  if ( *ctx->m_Cursor == ']' )
  {
    _gjLexSym* sym = gj_newSym( ctx );
    sym->m_Str     = ctx->m_Cursor;
    sym->m_StrLen  = 1;
    sym->m_Type    = kSymClosedBracket;
    ctx->m_Cursor++;
    return true;
  }
  return false;
}

//---------------------------------------------------------------------------------
bool gj_lexString( _gjLexContext* ctx )
{
  if ( *ctx->m_Cursor == '"' )
  {
    _gjLexSym* sym = gj_newSym( ctx );
    sym->m_Str     = ctx->m_Cursor + 1;
    sym->m_StrLen  = 0;
    sym->m_Type    = kSymString;
    ctx->m_Cursor++;

    while ( *ctx->m_Cursor != '"' || *(ctx->m_Cursor - 1) == '\\' )
    {
      sym->m_StrLen++;
      ctx->m_Cursor++;
    }

    ctx->m_Cursor++;

    return true;
  }

  return false;
}

//---------------------------------------------------------------------------------
bool gj_isDigit( char c )
{
  return c >= '0' && c <= '9';
}

//---------------------------------------------------------------------------------
bool gj_lexFloat( _gjLexContext* ctx )
{
  const char* cursor = ctx->m_Cursor;
  if ( gj_isDigit( *ctx->m_Cursor ) || *ctx->m_Cursor == '-' )
  {
    bool has_point = false;
    while ( gj_isDigit( *cursor )
        || *cursor == '-'
        || *cursor == '.'
        || *cursor == 'e'
        || *cursor == 'E' )
    {
      has_point |= (*cursor == '.');
      cursor++;
    }

    if ( has_point )
    {
      _gjLexSym* sym = gj_newSym( ctx );
      sym->m_Str     = ctx->m_Cursor;
      sym->m_StrLen  = (uint16_t)(cursor - ctx->m_Cursor);
      sym->m_Type    = kSymFloat;
      ctx->m_Cursor  = cursor;
      return true;
    }
  }

  return false;
}

//---------------------------------------------------------------------------------
bool gj_lexInt( _gjLexContext* ctx )
{
  char*   cursor  = nullptr;
  int32_t integer = strtol( ctx->m_Cursor, &cursor, 10 );

  if ( integer != 0 || cursor != ctx->m_Cursor )
  {
    if ( integer != LONG_MAX && integer != LONG_MIN )
    {
      _gjLexSym* sym = gj_newSym( ctx );
      sym->m_Str     = ctx->m_Cursor;
      sym->m_StrLen  = (uint16_t)( cursor - ctx->m_Cursor );
      sym->m_Type    = kSymInt;
      ctx->m_Cursor  = cursor;
      return true;
    }
  }

  return false;
}

//---------------------------------------------------------------------------------
bool gj_lexU64( _gjLexContext* ctx )
{
  char*    cursor  = nullptr;
  uint64_t integer = strtoull( ctx->m_Cursor, &cursor, 10 );

  if ( integer != 0 || cursor != ctx->m_Cursor )
  {
    if ( integer != LLONG_MAX && integer != LLONG_MIN )
    {
      _gjLexSym* sym = gj_newSym( ctx );
      sym->m_Str     = ctx->m_Cursor;
      sym->m_StrLen  = (uint16_t)( cursor - ctx->m_Cursor );
      sym->m_Type    = kSymU64;
      ctx->m_Cursor  = cursor;
      return true;
    }
  }

  return false;
}

//---------------------------------------------------------------------------------
bool gj_lexBool( _gjLexContext* ctx )
{
  if ( strncmp( ctx->m_Cursor, "true", 4 ) == 0 )
  {
    _gjLexSym* sym = gj_newSym( ctx );
    sym->m_Str     = ctx->m_Cursor;
    sym->m_StrLen  = 4;
    sym->m_Type    = kSymBool;
    ctx->m_Cursor  += 4;
    return true;
  }

  if ( strncmp( ctx->m_Cursor, "false", 5 ) == 0 )
  {
    _gjLexSym* sym = gj_newSym( ctx );
    sym->m_Str     = ctx->m_Cursor;
    sym->m_StrLen  = 5;
    sym->m_Type    = kSymBool;
    ctx->m_Cursor  += 5;
    return true;
  }

  return false;
}

//---------------------------------------------------------------------------------
bool gj_lexNull( _gjLexContext* ctx )
{
  if ( strncmp( ctx->m_Cursor, "null", 4 ) == 0 )
  {
    _gjLexSym* sym = gj_newSym( ctx );
    sym->m_Str     = ctx->m_Cursor;
    sym->m_StrLen  = 4;
    sym->m_Type    = kSymNull;
    ctx->m_Cursor  += 4;
    return true;
  }

  return false;
}

struct _gjAstNode;

//---------------------------------------------------------------------------------
struct _gjAstNodeMember
{
  char*    m_Key;
  uint32_t m_ValueIdx;
};

//---------------------------------------------------------------------------------
struct _gjAstNode
{
  enum NodeType : uint16_t
  {
    kTypeObject,
    kTypeMember,
    kTypeArray,
    kTypeString,
    kTypeInt,
    kTypeU64,
    kTypeFloat,
    kTypeBool,
    kTypeNull
  };

  union
  {
    uint32_t         m_ObjectStartIdx;
    _gjAstNodeMember m_Member;
    uint32_t         m_ArrayStartIdx;
    char*            m_String;
    int              m_Int;
    uint64_t         m_U64;
    float            m_Float;
    bool             m_Bool;
  };

  uint32_t m_Next;
  NodeType m_Type;
};

//---------------------------------------------------------------------------------
static constexpr uint32_t kAstNodeTailIdx = (uint32_t)-1;

//---------------------------------------------------------------------------------
struct _gjAstContext
{
  _gjAstNode* m_Nodes;
  uint32_t    m_NodeCount;
  uint32_t    m_NodeHead;
};

//---------------------------------------------------------------------------------
_gjAstNode* gj_allocAstNode( _gjAstContext* ctx, uint32_t* out_idx, uint32_t head = kAstNodeTailIdx )
{
  if ( ctx->m_NodeHead != kAstNodeTailIdx )
  {
    if ( head == kAstNodeTailIdx )
    {
      *out_idx = ctx->m_NodeHead;
      _gjAstNode* node = &ctx->m_Nodes[ ctx->m_NodeHead ];
      ctx->m_NodeHead = node->m_Next;
      node->m_Next = kAstNodeTailIdx;

      return node;
    }

    _gjAstNode* prev_node = &ctx->m_Nodes[ head ];
    while ( prev_node->m_Next != kAstNodeTailIdx )
    {
      prev_node = &ctx->m_Nodes[ prev_node->m_Next ];
    }

    prev_node->m_Next = ctx->m_NodeHead;
    *out_idx = prev_node->m_Next;
    _gjAstNode* new_node = &ctx->m_Nodes[ prev_node->m_Next ];
    ctx->m_NodeHead = new_node->m_Next;
    new_node->m_Next = kAstNodeTailIdx;

    return new_node;
  }

  return nullptr;
}

//---------------------------------------------------------------------------------
uint32_t gj_parse( _gjLexContext* lex, _gjAstContext* ast );

//---------------------------------------------------------------------------------
uint32_t gj_parseMember( _gjLexContext* lex, _gjAstContext* ast )
{
  _gjLexSym* key_str_sym = &lex->m_Syms[ lex->m_ReadIdx++ ];
  if ( key_str_sym->m_Type != kSymString )
  {
    gj_assert( "Unexpected token!" );
    return (uint32_t)-1;
  }

  uint32_t member_idx;
  _gjAstNode* member_node = gj_allocAstNode( ast, &member_idx );
  
  member_node->m_Member.m_Key = (char*)gj_malloc( key_str_sym->m_StrLen + 1, "AST key string" );
  memcpy( member_node->m_Member.m_Key, key_str_sym->m_Str, key_str_sym->m_StrLen );
  member_node->m_Member.m_Key[ key_str_sym->m_StrLen ] = '\0';
  
  if ( lex->m_Syms[ lex->m_ReadIdx++ ].m_Type != kSymColon )
  {
    gj_assert( "Unexpected token!" );
    return (uint32_t)-1;
  }
  
  member_node->m_Member.m_ValueIdx = gj_parse( lex, ast );
  member_node->m_Next = kAstNodeTailIdx;

  return member_idx;
}

//---------------------------------------------------------------------------------
bool gj_isSymValueType( _gjLexSymType type )
{
  return type == kSymBool        ||
         type == kSymFloat       ||
         type == kSymInt         ||
         type == kSymU64         ||
         type == kSymNull        ||
         type == kSymOpenBrace   ||
         type == kSymOpenBracket ||
         type == kSymString;
}

//---------------------------------------------------------------------------------
uint32_t gj_cStringLen( const char* json_str, uint32_t json_len )
{
  uint32_t collapse_amt = 0;
  for ( uint32_t i_char = 0; i_char < json_len; ++i_char )
  {
    collapse_amt += json_str[ i_char + collapse_amt ] == '\\';
  }

  return json_len - collapse_amt;
}

//---------------------------------------------------------------------------------
// returns 0 for ok
uint32_t gj_jsonToCString( char* c_string, uint32_t c_string_len, const char* json_str )
{
  // todo: vectorizable?
  uint32_t offset_amt = 0;
  for ( uint32_t i_char = 0; i_char < c_string_len + 1; ++i_char )
  {
    const uint32_t i_char_src = i_char + offset_amt;
    if (json_str[ i_char_src ] == '\\' )
    {
      offset_amt++;
      switch (json_str[ i_char_src + 1 ] )
      {
        case '"': // These just grab the next char
        case '\\':
        case '/':
        {
          c_string[ i_char ] = json_str[ i_char_src + 1 ];
        }
        break;
        case 'n':
        {
          c_string[ i_char ] = '\n';
        }
        break;
        case 'r':
        {
          c_string[ i_char ] = '\r';
        }
        break;
        case 't':
        {
          c_string[ i_char ] = '\t';
        }
        break;
        case 'b':
        {
          c_string[ i_char ] = '\b';
        }
        break;
        case 'f':
        {
          c_string[ i_char ] = '\f';
        }
        break;
        default:
        {
          gj_assert( "error parsing string literal: unrecognized escape sequence" );
          return (uint32_t)-1;
        }
      }
    }
    else
    {
      c_string[ i_char ] = json_str[ i_char_src ];
    }
  } 

  return 0;
}

//---------------------------------------------------------------------------------
// returns index into ast node array
uint32_t gj_parse( _gjLexContext* lex, _gjAstContext* ast )
{
  _gjLexSym* sym = &lex->m_Syms[ lex->m_ReadIdx++ ];
  switch ( sym->m_Type )
  {
    case kSymInt:
    {
      uint32_t idx;
      _gjAstNode* node = gj_allocAstNode( ast, &idx );
      node->m_Type = _gjAstNode::kTypeInt;
      node->m_Int  = strtol( sym->m_Str, nullptr, 10 );
      return idx;
    }
    break;
    case kSymU64:
    {
      uint32_t idx;
      _gjAstNode* node = gj_allocAstNode( ast, &idx );
      node->m_Type = _gjAstNode::kTypeU64;
      node->m_U64  = strtoull( sym->m_Str, nullptr, 10 );
      return idx;
    }
    break;
    case kSymFloat:
    {
      uint32_t idx;
      _gjAstNode* node = gj_allocAstNode( ast, &idx );
      node->m_Type  = _gjAstNode::kTypeFloat;
      node->m_Float = strtof( sym->m_Str, nullptr );
      return idx;
    }
    break;
    case kSymBool:
    {
      uint32_t idx;
      _gjAstNode* node = gj_allocAstNode( ast, &idx );
      node->m_Type  = _gjAstNode::kTypeBool;
      node->m_Bool  = strncmp( sym->m_Str, "true", 4 ) == 0;
      return idx;
    }
    break;
    case kSymString:
    {
      uint32_t idx;
      _gjAstNode* node = gj_allocAstNode( ast, &idx );
      node->m_Type   = _gjAstNode::kTypeString;

      
      const uint32_t c_string_len = gj_cStringLen( sym->m_Str, sym->m_StrLen );

      node->m_String = (char*)gj_malloc( c_string_len + 1, "AST string value" );

      if ( gj_jsonToCString( node->m_String, c_string_len, sym->m_Str ) != 0 )
      {
        gj_free( node->m_String );
        return (uint32_t)-1;
      }

      node->m_String[ c_string_len ] = '\0';

      return idx;
    }
    break;
    case kSymNull:
    {
      uint32_t idx;
      _gjAstNode* node = gj_allocAstNode( ast, &idx );
      node->m_Type  = _gjAstNode::kTypeNull;
      return idx;
    }
    break;
    case kSymOpenBrace:
    {
      uint32_t obj_idx;
      _gjAstNode* obj_node = gj_allocAstNode( ast, &obj_idx );
      obj_node->m_Type           = _gjAstNode::kTypeObject;
      obj_node->m_ObjectStartIdx = kAstNodeTailIdx;

      sym = &lex->m_Syms[ lex->m_ReadIdx ];
      if (sym->m_Type == kSymString )
      {
        obj_node->m_ObjectStartIdx = gj_parseMember( lex, ast );
        if ( obj_node->m_ObjectStartIdx == kAstNodeTailIdx)
        {
          return (uint32_t)-1;
        }
        _gjAstNode* prev_node = &ast->m_Nodes[ obj_node->m_ObjectStartIdx ];

        sym = &lex->m_Syms[ lex->m_ReadIdx ];
        while ( sym->m_Type == kSymComma )
        {
          lex->m_ReadIdx++;
          prev_node->m_Next = gj_parseMember( lex, ast );
          
          if ( prev_node->m_Next == kAstNodeTailIdx)
          {
            return (uint32_t)-1;
          }
          prev_node = &ast->m_Nodes[ prev_node->m_Next ];
          sym = &lex->m_Syms[ lex->m_ReadIdx ];
        }
      }

      if ( sym->m_Type != kSymClosedBrace )
      {
        gj_assert( "unrecognized token!" );
        return (uint32_t)-1;
      }

      lex->m_ReadIdx++;

      return obj_idx;
    }
    break;
    case kSymOpenBracket:
    {
      uint32_t arr_idx;
      _gjAstNode* arr_node      = gj_allocAstNode( ast, &arr_idx );
      arr_node->m_Type          = _gjAstNode::kTypeArray;
      arr_node->m_ArrayStartIdx = kAstNodeTailIdx;

      sym = &lex->m_Syms[ lex->m_ReadIdx ];
      if ( gj_isSymValueType( sym->m_Type ) )
      {
        arr_node->m_ArrayStartIdx = gj_parse( lex, ast );
        _gjAstNode* prev_node = &ast->m_Nodes[ arr_node->m_ArrayStartIdx ];
        prev_node->m_Next = kAstNodeTailIdx;

        sym = &lex->m_Syms[ lex->m_ReadIdx++ ];
        while ( sym->m_Type == kSymComma )
        {
          if ( gj_isSymValueType( lex->m_Syms[ lex->m_ReadIdx ].m_Type ) == false )
          {
            gj_assert( "unrecognized symbol!" );
            return ( uint32_t )-1;
          }

          prev_node->m_Next = gj_parse( lex, ast );
          prev_node = &ast->m_Nodes[ prev_node->m_Next ];
          prev_node->m_Next = kAstNodeTailIdx;

          sym = &lex->m_Syms[ lex->m_ReadIdx++ ];
        }
      }
      else if ( sym->m_Type == kSymClosedBracket )
      {
        lex->m_ReadIdx++;
      }

      if ( sym->m_Type != kSymClosedBracket )
      {
        gj_assert( "unrecognized token!" );
        return (uint32_t)-1;
      }

      return arr_idx;

    }
    break;
    default:
    gj_assert( "Unexpected token!" );
  }

  return (uint32_t)-1;
}

//---------------------------------------------------------------------------------
gjValue gj_astToValue( _gjAstContext* ast, uint32_t node_idx )
{
  _gjAstNode* node = &ast->m_Nodes[ node_idx ];
  switch ( node->m_Type )
  {
    case _gjAstNode::kTypeString:
    {
      gjValue val = gjValue( node->m_String );
      gj_free( node->m_String );
      return val;
    }
    break;
    case _gjAstNode::kTypeInt:
    {
      return gjValue( node->m_Int );
    }
    break;
    case _gjAstNode::kTypeU64:
    {
      return gjValue( node->m_U64 );
    }
    break;
    case _gjAstNode::kTypeFloat:
    {
      return gjValue( node->m_Float );
    }
    break;
    case _gjAstNode::kTypeBool:
    {
      return gjValue( node->m_Bool );
    }
    break;
    case _gjAstNode::kTypeNull:
    {
      return gjValue();
    }
    break;
    case _gjAstNode::kTypeObject:
    {
      gjValue obj = gj_makeObject();
      if ( node->m_ObjectStartIdx != kAstNodeTailIdx )
      {
        _gjAstNode* member_node = &ast->m_Nodes[ node->m_ObjectStartIdx ];
        while ( member_node->m_Next != kAstNodeTailIdx )
        {
          char* key_str = member_node->m_Member.m_Key;

          obj.addMember( key_str, gj_astToValue( ast, member_node->m_Member.m_ValueIdx ) );
          gj_free( key_str );

          member_node = &ast->m_Nodes[ member_node->m_Next ];
        }

        char* key_str = member_node->m_Member.m_Key;

        obj.addMember( key_str, gj_astToValue( ast, member_node->m_Member.m_ValueIdx ) );
        gj_free( key_str );
      }

      return obj;
    }
    break;
    case _gjAstNode::kTypeMember:
    {
      gj_assert( "malformed ast! This is a bug in the library" );
    }
    break;
    case _gjAstNode::kTypeArray:
    {
      gjValue arr = gj_makeArray();

      if ( node->m_ArrayStartIdx != kAstNodeTailIdx )
      {
        uint32_t    elem_idx  = node->m_ArrayStartIdx;
        _gjAstNode* elem_node = &ast->m_Nodes[ elem_idx ];
        while ( elem_node->m_Next != kAstNodeTailIdx )
        {
          arr.insertElement( gj_astToValue( ast, elem_idx ) );
          elem_idx  = elem_node->m_Next;
          elem_node = &ast->m_Nodes[ elem_idx ];
        }

        arr.insertElement( gj_astToValue( ast, elem_idx ) );
      }

      return arr;
    }
    break;
  }

  return gjValue{};
}

//---------------------------------------------------------------------------------
gjValue gj_parse( const char* json_string, size_t string_len )
{
  _gjLexContext lex_ctx;
  lex_ctx.m_Cursor   = json_string;
  lex_ctx.m_MaxLen   = string_len;
  lex_ctx.m_Syms     = (_gjLexSym*)gj_malloc( sizeof( *lex_ctx.m_Syms ) * lex_ctx.m_MaxLen, "Lexer scratch" );
  lex_ctx.m_SymCount = 0;
  lex_ctx.m_ReadIdx  = 0;

  while ( *lex_ctx.m_Cursor )
  {
    const bool valid_lex = 
       gj_lexWhitespace   ( &lex_ctx ) ||
       gj_lexOpenBrace    ( &lex_ctx ) ||
       gj_lexClosedBrace  ( &lex_ctx ) ||
       gj_lexComma        ( &lex_ctx ) ||
       gj_lexColon        ( &lex_ctx ) ||
       gj_lexOpenBracket  ( &lex_ctx ) ||
       gj_lexClosedBracket( &lex_ctx ) ||
       gj_lexString       ( &lex_ctx ) ||
       gj_lexFloat        ( &lex_ctx ) ||
       gj_lexInt          ( &lex_ctx ) ||
       gj_lexU64          ( &lex_ctx ) ||
       gj_lexBool         ( &lex_ctx ) ||
       gj_lexNull         ( &lex_ctx );

    if ( valid_lex == false )
    {
      gj_assert( "unrecognized format!" );
    }
  }

  _gjAstContext ast_ctx;
  ast_ctx.m_Nodes     = (_gjAstNode*)gj_malloc( sizeof( *ast_ctx.m_Nodes ) * lex_ctx.m_SymCount, "AST Node Scratch" );
  ast_ctx.m_NodeCount = lex_ctx.m_SymCount;
  ast_ctx.m_NodeHead  = 0;

  for ( uint32_t i_node = 0; i_node < ast_ctx.m_NodeCount; ++i_node )
  {
    ast_ctx.m_Nodes[ i_node ].m_Next = i_node + 1;
  }
  ast_ctx.m_Nodes[ ast_ctx.m_NodeCount - 1 ].m_Next = kAstNodeTailIdx;

  if ( gj_parse( &lex_ctx, &ast_ctx ) == (uint32_t)-1 )
  {
    gj_free( lex_ctx.m_Syms );
    for ( uint32_t i_node = 0; i_node < ast_ctx.m_NodeCount; ++i_node )
    {
      _gjAstNode* node = &ast_ctx.m_Nodes[ i_node ];
      if ( node->m_Type == _gjAstNode::kTypeString )
      {
        gj_free( node->m_String );
      }
      else if ( node->m_Type == _gjAstNode::kTypeMember )
      {
        gj_free( node->m_Member.m_Key );
      }
    }
    gj_free( ast_ctx.m_Nodes );
    return gjValue{};
  }

  gj_free( lex_ctx.m_Syms );

  gjValue value = gj_astToValue( &ast_ctx, 0 );

  gj_free( ast_ctx.m_Nodes );

  return value;
}