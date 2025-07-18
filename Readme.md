# goodjson is a JSON parser/serializer built with a focus on memory efficiency and performance

The goal of goodjson is to offer predictable memory usage for use cases where memory is under tight control

Speed is a secondary concern, but is of higher priority than ease-of-use (though I think this isn't so bad either)

# quickstart

Initialize goodjson by setting up a configuration, and calling init:

```
gjConfig gj_config = gj_getDefaultConfig();
gj_init( &gj_config );
```

This will make an initial allocation, which serves as the backing for all JSON values. 
This means you can only instantiate a certain number of objects across your entire application.
You may configure how many this is, by setting `gj_config.max_value_count`

From here you can parse a JSON string:

```
const char json_str[] =
  "{\n"
  "  \"my_int\"  : -200,\n"
  "  \"my_bool\" : true,\n"
  "  \"my_arr\" : [\n"
  "    \"one\",\n"
  "    \"two\",\n"
  "    \"three\"\n"
  "  ],\n"
  "  \"my_arr2\": [\n"
  "    \"thingy\",\n"
  "    \"thingie\"\n"
  "  ]\n"
  "}";

gjValue parsed = gj_parse( json_str, sizeof( json_str ) );
```

modify the data:

```
parsed[ "my_int" ] = 200;
```

and serialize it out again:

```
{
  gjSerializer serializer( parsed );
  serializer.serialize();
  printf( "%s\n", serializer.getString() );
}
```

when you are done with your object, it is recommended to destroy the resource. 
This will delete all values, and allocations that those values own, under this value:

```
gj_deleteValue( parsed );
```

don't forget to shutdown the system at the end of your program

```
gj_shutdown();
```

# memory usage

Every object is stored in a backing array. Strings and key names are allocated ad-hoc.
When you get a value from an object, a reference is returned, rather than a copy/value.
If you would like to make a duplicate json structure, you can call `gjValue val = value.deepCopy()`

You can keep an eye on your usage of the backing resources by calling

```
gjUsageStats stats = gj_getUsageStats();
```

A call to `gj_parse()` will temporarily allocate a big chunk of memory for the lexer symbols and AST. The amount allocated depends on the size of your input string. When gj_parse is done, it frees this memory

the `gjSerializer` is the only object that utilizes RAII semantics in the library. The serializer will temporarily allocate string data that can be read using `serializer.getString()`. Once the object goes out of scope, the backing string data is freed.


# custom allocators

If you would like to use your own allocator, it is very simple.
You must do this before calling `gj_init()`. To do so, simply do the following:

```
gjAllocatorHooks alloc_hooks;
alloc_hooks.mallocFn = my_malloc;
alloc_hooks.freeFn   = my_free;

gj_setAllocator( &alloc_hooks );
```

# custom assert function

You may also provide your own assert function 
```
void gj_setAssertFn( myAssertFn );
```


# other examples

## creating an object from scratch

```
gjValue arr = gj_makeArray();
arr.insertElement( gjValue( "one"   ), 0 );
arr.insertElement( gjValue( "three" ), 2 );
arr.insertElement( gjValue( "two"   ), 1 );

gjValue my_obj = gj_makeObject();
my_obj.addMember( "my_float", gjValue( 3.141592f ) );
my_obj.addMember( "my_str",   gjValue( "bongus" ) );

gjValue obj = gj_makeObject();
obj.addMember( "my_int",  gjValue( -200 ) );
obj.addMember( "my_arr",  arr );
obj.addMember( "my_bool", gjValue( true ) );
obj.addMember( "my_object", my_obj );

{
  gjSerializer serializer( obj );
  serializer.serialize();
  printf( "%s\n", serializer.getString() );
}

gj_deleteValue( obj );

```

## useful usage printing

```
void printUsageStats()
{
  gjUsageStats stats = gj_getUsageStats();
  printf( "gj usage stats-------------------------------------------------------\n" );
  printf( "values         used: %lu free: %lu\n", stats.m_UsedValues, stats.m_FreeValues );
  printf( "array elems    used: %lu free: %lu\n", stats.m_UsedArrayElements, stats.m_FreeArrayElements );
  printf( "object members used: %lu free: %lu\n", stats.m_UsedObjectMembers, stats.m_FreeObjectMembers );
  printf( "-------------------------------------------------------------\n\n" );
}

```
