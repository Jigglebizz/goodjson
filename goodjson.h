#pragma once

#include <stdint.h>

//---------------------------------------------------------------------------------
struct gjConfig
{
  uint32_t max_value_count;
};

//---------------------------------------------------------------------------------
gjConfig gj_getDefaultConfig();

//---------------------------------------------------------------------------------
// Must be called before any functionality.
// If desired, custom allocators should be set up prior to calling this
void gj_init    ( const gjConfig* config );
void gj_shutdown();

//---------------------------------------------------------------------------------
static constexpr uint32_t kArrayIndexEnd = (uint32_t)-1;

//---------------------------------------------------------------------------------
enum class gjValueType : uint8_t
{
  kNull,
  kObject,
  kArray,
  kString,
  kNumber,
  kBool,

  kCount,
  kInvalid = (uint8_t)-1
};

//---------------------------------------------------------------------------------
struct gjValue
{
  uint32_t idx; // Do not edit these
  uint32_t gen; // Do not edit these

           gjValue();
  explicit gjValue( int v );
  explicit gjValue( uint64_t v );
  explicit gjValue( float v );
  explicit gjValue( bool v );
  explicit gjValue( const char* v );

  gjValue operator[]( uint32_t idx ) const;
  gjValue operator[]( const char* key ) const;

  gjValueType getType  () const;

  int         getInt   () const;
  uint64_t    getU64   () const;
  float       getFloat () const;
  const char* getString() const;
  bool        getBool  () const;

  void        setInt   ( int v );
  void        setU64   ( uint64_t v );
  void        setFloat ( float v );
  void        setString( const char* str );
  void        setBool  ( bool v );
  void        setNull  ();

  gjValue&    operator=( int v );
  gjValue&    operator=( uint64_t v );
  gjValue&    operator=( float v );
  gjValue&    operator=( const char* v );
  gjValue&    operator=( bool v );

  // This is for arrays, not objects
  uint32_t    getElementCount() const;
  gjValue     getElement     ( uint32_t elem_idx ) const;

  void        insertElement  ( gjValue val, uint32_t insert_idx = kArrayIndexEnd );
  void        removeElement  ( uint32_t remove_idx );
  void        clearArray     ();
  
  // This is for objects, not arrays
  uint32_t    getMemberCount() const;
  gjValue     getMember     ( const char* key       ) const;
  gjValue     getMember     ( uint32_t    key_crc32 ) const;
  bool        hasMember     ( const char* key       ) const;
  bool        hasMember     ( uint32_t    key_crc32 ) const;

  void        addMember     ( const char* key, gjValue value );
  void        removeMember  ( const char* key       );
  void        removeMember  ( uint32_t    key_crc32 );
  void        clearObject   ();

  gjValue     makeDeepCopy  ();
};

//---------------------------------------------------------------------------------
gjValue     gj_parse        ( const char* json_string, size_t string_len );
gjValue     gj_makeArray    ();
gjValue     gj_makeObject   ();
void        gj_deleteValue  ( gjValue val );

//---------------------------------------------------------------------------------
enum class gjSerializeMode : uint32_t
{
  kPretty,
  kMinified,

  kCount
};

//---------------------------------------------------------------------------------
enum class gjNewlineStyle : uint32_t
{
  kWindows,
  kLinux,

  kCount
};

//---------------------------------------------------------------------------------
static constexpr int kGjIndentAmtTabs = -1;
struct gjSerializeOptions
{
  gjSerializeMode mode;
  gjNewlineStyle  newline_style;
  int             indent_amt;
};

gjSerializeOptions gj_getDefaultSerializeOptions();

//---------------------------------------------------------------------------------
class gjSerializer
{
public:
  gjSerializer ( gjValue obj_to_serialize, gjSerializeOptions* options = nullptr );
  ~gjSerializer();

  void        serialize();
  const char* getString();
private:
  gjValue            m_Obj;
  gjSerializeOptions m_Options;
  char*              m_StringData;
};

//---------------------------------------------------------------------------------
// 
// Allocator customization
// 
//---------------------------------------------------------------------------------
typedef void*(*gjMallocFn)  ( size_t sz, const char* description );
typedef void (*gjFreeFn)    ( void* ptr );
typedef void (*gjAssertFn)  ( const char* message );

//---------------------------------------------------------------------------------
struct gjAllocatorHooks
{
  gjMallocFn mallocFn;
  gjFreeFn   freeFn;
};

//---------------------------------------------------------------------------------
void gj_setAllocator( const gjAllocatorHooks* hooks );

//---------------------------------------------------------------------------------
//
// Usage stats
//
//---------------------------------------------------------------------------------
struct gjUsageStats
{
  uint32_t m_UsedValues;
  uint32_t m_FreeValues;
  uint32_t m_UsedArrayElements;
  uint32_t m_FreeArrayElements;
  uint32_t m_UsedObjectMembers;
  uint32_t m_FreeObjectMembers;
};

gjUsageStats gj_getUsageStats();