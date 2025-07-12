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
  const char* m_KeyStr;
  gjValue     m_Value;
  uint32_t    m_Gen;
  uint32_t    m_Next;
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
enum gjSubValueType : uint32_t
{
  kGjSubValueTypeU64,
  kGjSubValueTypeInt,
  kGjSubValueTypeFloat,

  kGjSubValueTypeCount,
  kGjSubValueTypeInvalid = (uint32_t)-1
};

//---------------------------------------------------------------------------------
struct _gjValue
{
  gjValueType    m_Type;
  gjSubValueType m_SubType;
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
  uint32_t    m_Gen;
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
_gjArrayElem* gj_allocArrayElem( uint32_t* out_idx, uint32_t array_idx= kArrayIndexEnd, uint32_t head=kArrayIdxTail )
{
  *out_idx = kArrayIdxTail;
  if ( s_ArrayPoolHead != kArrayIdxTail )
  {
    if ( head == kArrayIdxTail || array_idx == 0 )
    {
      *out_idx = s_ArrayPoolHead;
      s_ArrayPoolHead = s_ArrayPool[ s_ArrayPoolHead ].m_Next;
      s_ArrayPool[ *out_idx ].m_Next = head;
      return &s_ArrayPool[ *out_idx ];
    }
    else
    {
      _gjArrayElem* elem = &s_ArrayPool[ head ];
      uint32_t      idx  = 1;
      while ( elem->m_Next != kArrayIdxTail && idx++ != array_idx )
      {
        elem = &s_ArrayPool[ elem->m_Next ];
      }
      *out_idx = s_ArrayPoolHead;
      s_ArrayPoolHead = s_ArrayPool[ s_ArrayPoolHead ].m_Next;
      
      const uint32_t prev_next = elem->m_Next;
      elem->m_Next = *out_idx;
      s_ArrayPool[ *out_idx ].m_Next = prev_next;

      return &s_ArrayPool[ *out_idx ];
    }
  }
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
  }
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
  if ( val->m_Type == gjValueType::kString )
  {
    gj_free( val->m_Str );
  }
  else if ( val->m_Type == gjValueType::kArray )
  {
    _gjArrayHandle arr_start_handle = val->m_ArrayStart;
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
  // todo: free object
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
    val->m_Type    = gjValueType::kNumber;
    val->m_SubType = kGjSubValueTypeInt;
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
    val->m_Type    = gjValueType::kNumber;
    val->m_SubType = kGjSubValueTypeU64;
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
    val->m_Type    = gjValueType::kNumber;
    val->m_SubType = kGjSubValueTypeFloat;
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
    val->m_Type    = gjValueType::kBool;
    val->m_SubType = kGjSubValueTypeInvalid;
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
    val->m_Type = gjValueType::kString;
    val->m_SubType = kGjSubValueTypeInvalid;

    size_t str_len = gj_StrLen( v );
    if ( val->m_Str = (char*)gj_malloc( str_len, "Value String" ) )
    {
      memcpy( val->m_Str, v, str_len );
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
    return val->m_Type;
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
    if ( val->m_Type == gjValueType::kNumber)
    {
      switch ( val->m_SubType )
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
    if ( val->m_Type == gjValueType::kNumber )
    {
      switch ( val->m_SubType )
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
    if ( val->m_Type == gjValueType::kNumber )
    {
      switch ( val->m_SubType )
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
    if (  val->m_Type == gjValueType::kString )
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
    if ( val->m_Type == gjValueType::kString )
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

    val->m_Type    = gjValueType::kNumber;
    val->m_SubType = kGjSubValueTypeInt;
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

    val->m_Type    = gjValueType::kNumber;
    val->m_SubType = kGjSubValueTypeU64;
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

    val->m_Type    = gjValueType::kNumber;
    val->m_SubType = kGjSubValueTypeFloat;
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

    val->m_Type    = gjValueType::kString;
    val->m_SubType = kGjSubValueTypeInvalid;

    const size_t str_len = gj_StrLen( str );
    val->m_Str     = (char*)gj_malloc( str_len, "setString string" );
    memcpy( val->m_Str, str, str_len );
  }
}

//---------------------------------------------------------------------------------
void gjValue::setBool( bool v )
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    _gjValue* val = &s_ValuePool[ idx ];
    gj_freeValueData( val );

    val->m_Type    = gjValueType::kBool;
    val->m_SubType = kGjSubValueTypeInvalid;
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

    val->m_Type    = gjValueType::kNull;
    val->m_SubType = kGjSubValueTypeInvalid;
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
// Array operations
//
//---------------------------------------------------------------------------------
uint32_t gjValue::getElementCount() const
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    const _gjValue* val = &s_ValuePool[ idx ];
    if ( val->m_Type == gjValueType::kArray )
    {
      const _gjArrayHandle arr_handle_start = val->m_ArrayStart;
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
    if ( val->m_Type == gjValueType::kArray )
    {
      const _gjArrayHandle arr_handle_start = val->m_ArrayStart;
      if ( arr_handle_start.m_Gen == s_ArrayPool[ arr_handle_start.m_Idx ].m_Gen )
      {
        uint32_t cur_idx = 0;
        const _gjArrayElem* elem = &s_ArrayPool[ cur_idx ];
        while ( cur_idx != elem_idx )
        {
          cur_idx = elem->m_Next;
          elem = &s_ArrayPool[ cur_idx ];
        }

        return elem->m_Value;
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
void gjValue::insertElement( gjValue val, uint32_t insert_idx /* = kArrayIndexEnd */ )
{
  if ( gj_isValueAlloced( idx, gen ) )
  {
    if ( s_ValuePool[ idx ].m_Type == gjValueType::kArray )
    {
      uint32_t arr_idx;
      gj_allocArrayElem( &arr_idx, insert_idx, s_ValuePool[ idx ].m_ArrayStart.m_Idx );

      if ( arr_idx != kArrayIdxTail )
      {
        // The head has been replaced, update the head value
        if ( s_ValuePool[ idx ].m_ArrayStart.m_Idx == kArrayIdxTail )
        {
          s_ValuePool[ idx ].m_ArrayStart.m_Idx = arr_idx;
        }

        s_ArrayPool[ idx ].m_Value = val;
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
    if ( s_ValuePool[ idx ].m_Type == gjValueType::kArray )
    {
      const _gjArrayElem* freed_elem  = gj_freeArrayElem( &s_ValuePool[ idx ].m_ArrayStart, remove_idx );
      if ( gj_isValueAlloced( freed_elem->m_Value.idx, freed_elem->m_Value.gen ) )
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
    if ( s_ValuePool[ idx ].m_Type == gjValueType::kArray )
    {
      gj_freeValueData( &s_ValuePool[ idx ] );
      _gjValue* val = &s_ValuePool[ idx ];
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
    if ( val->m_Type == gjValueType::kObject )
    {
      const _gjMemberHandle obj_handle_start = val->m_ObjectStart;
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
gjValue gjValue::getMember( const char* key ) const
{
  
}

//---------------------------------------------------------------------------------
bool gjValue::hasMember( const char* key ) const
{
  
}

//---------------------------------------------------------------------------------
void gjValue::addMember( const char* key )
{

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
    val->m_Type    = gjValueType::kArray;
    val->m_SubType = kGjSubValueTypeInvalid;
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
    val->m_Type    = gjValueType::kObject;
    val->m_SubType = kGjSubValueTypeInvalid;
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
    gj_freeValue( val.idx );
  }
}