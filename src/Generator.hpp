#ifndef GENERATOR_HPP
#define GENERATOR_HPP

#include "XMLUtils.hpp"
#include "XMLVariableParser.hpp"
#include "tinyxml2.h"
#include <filesystem>
#include <forward_list>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

static constexpr char const *RES_HEADER{
    R"(
#if !defined( VULKAN_HPP_ASSERT_ON_RESULT )
#  define VULKAN_HPP_ASSERT_ON_RESULT VULKAN_HPP_ASSERT
#endif

#if !defined( VULKAN_HPP_STATIC_ASSERT )
#  define VULKAN_HPP_STATIC_ASSERT static_assert
#endif

static_assert(VK_HEADER_VERSION == {0}, "Wrong VK_HEADER_VERSION!");

// 32-bit vulkan is not typesafe for handles, so don't allow copy constructors on this platform by default.
// To enable this feature on 32-bit platforms please define VULKAN_HPP_TYPESAFE_CONVERSION
#if ( VK_USE_64_BIT_PTR_DEFINES == 1 )
#  if !defined( VULKAN_HPP_TYPESAFE_CONVERSION )
#    define VULKAN_HPP_TYPESAFE_CONVERSION
#  endif
#endif

// <tuple> includes <sys/sysmacros.h> through some other header
// this results in major(x) being resolved to gnu_dev_major(x)
// which is an expression in a constructor initializer list.
#if defined( major )
#  undef major
#endif
#if defined( minor )
#  undef minor
#endif

// Windows defines MemoryBarrier which is deprecated and collides
// with the VULKAN_HPP_NAMESPACE::MemoryBarrier struct.
#if defined( MemoryBarrier )
#  undef MemoryBarrier
#endif

#if !defined( VULKAN_HPP_HAS_UNRESTRICTED_UNIONS )
#  if defined( __clang__ )
#    if __has_feature( cxx_unrestricted_unions )
#      define VULKAN_HPP_HAS_UNRESTRICTED_UNIONS
#    endif
#  elif defined( __GNUC__ )
#    define GCC_VERSION ( __GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__ )
#    if 40600 <= GCC_VERSION
#      define VULKAN_HPP_HAS_UNRESTRICTED_UNIONS
#    endif
#  elif defined( _MSC_VER )
#    if 1900 <= _MSC_VER
#      define VULKAN_HPP_HAS_UNRESTRICTED_UNIONS
#    endif
#  endif
#endif

#if !defined( VULKAN_HPP_INLINE )
#  if defined( __clang__ )
#    if __has_attribute( always_inline )
#      define VULKAN_HPP_INLINE __attribute__( ( always_inline ) ) __inline__
#    else
#      define VULKAN_HPP_INLINE inline
#    endif
#  elif defined( __GNUC__ )
#    define VULKAN_HPP_INLINE __attribute__( ( always_inline ) ) __inline__
#  elif defined( _MSC_VER )
#    define VULKAN_HPP_INLINE inline
#  else
#    define VULKAN_HPP_INLINE inline
#  endif
#endif

#if defined( VULKAN_HPP_TYPESAFE_CONVERSION )
#  define VULKAN_HPP_TYPESAFE_EXPLICIT
#else
#  define VULKAN_HPP_TYPESAFE_EXPLICIT explicit
#endif

#if defined( __cpp_constexpr )
#  define VULKAN_HPP_CONSTEXPR constexpr
#  if __cpp_constexpr >= 201304
#    define VULKAN_HPP_CONSTEXPR_14 constexpr
#  else
#    define VULKAN_HPP_CONSTEXPR_14
#  endif
#  define VULKAN_HPP_CONST_OR_CONSTEXPR constexpr
#else
#  define VULKAN_HPP_CONSTEXPR
#  define VULKAN_HPP_CONSTEXPR_14
#  define VULKAN_HPP_CONST_OR_CONSTEXPR const
#endif

#if !defined( VULKAN_HPP_NOEXCEPT )
#  if defined( _MSC_VER ) && ( _MSC_VER <= 1800 )
#    define VULKAN_HPP_NOEXCEPT
#  else
#    define VULKAN_HPP_NOEXCEPT     noexcept
#    define VULKAN_HPP_HAS_NOEXCEPT 1
#    if defined( VULKAN_HPP_NO_EXCEPTIONS )
#      define VULKAN_HPP_NOEXCEPT_WHEN_NO_EXCEPTIONS noexcept
#    else
#      define VULKAN_HPP_NOEXCEPT_WHEN_NO_EXCEPTIONS
#    endif
#  endif
#endif

#if 14 <= VULKAN_HPP_CPP_VERSION
#  define VULKAN_HPP_DEPRECATED( msg ) [[deprecated( msg )]]
#else
#  define VULKAN_HPP_DEPRECATED( msg )
#endif

#if ( 17 <= VULKAN_HPP_CPP_VERSION ) && !defined( VULKAN_HPP_NO_NODISCARD_WARNINGS )
#  define VULKAN_HPP_NODISCARD [[nodiscard]]
#  if defined( VULKAN_HPP_NO_EXCEPTIONS )
#    define VULKAN_HPP_NODISCARD_WHEN_NO_EXCEPTIONS [[nodiscard]]
#  else
#    define VULKAN_HPP_NODISCARD_WHEN_NO_EXCEPTIONS
#  endif
#else
#  define VULKAN_HPP_NODISCARD
#  define VULKAN_HPP_NODISCARD_WHEN_NO_EXCEPTIONS
#endif
)"};

static constexpr char const *RES_ERRORS{R"(
  class ErrorCategoryImpl : public std::error_category
  {
  public:
    virtual const char * name() const VULKAN_HPP_NOEXCEPT override
    {
      return VULKAN_HPP_NAMESPACE_STRING "::Result";
    }
    virtual std::string message( int ev ) const override
    {
      return to_string( static_cast<Result>( ev ) );
    }
  };

  class Error
  {
  public:
    Error() VULKAN_HPP_NOEXCEPT                = default;
    Error( const Error & ) VULKAN_HPP_NOEXCEPT = default;
    virtual ~Error() VULKAN_HPP_NOEXCEPT       = default;

    virtual const char * what() const VULKAN_HPP_NOEXCEPT = 0;
  };

  class LogicError
    : public Error
    , public std::logic_error
  {
  public:
    explicit LogicError( const std::string & what ) : Error(), std::logic_error( what ) {}
    explicit LogicError( char const * what ) : Error(), std::logic_error( what ) {}

    virtual const char * what() const VULKAN_HPP_NOEXCEPT
    {
      return std::logic_error::what();
    }
  };

  class SystemError
    : public Error
    , public std::system_error
  {
  public:
    SystemError( std::error_code ec ) : Error(), std::system_error( ec ) {}
    SystemError( std::error_code ec, std::string const & what ) : Error(), std::system_error( ec, what ) {}
    SystemError( std::error_code ec, char const * what ) : Error(), std::system_error( ec, what ) {}
    SystemError( int ev, std::error_category const & ecat ) : Error(), std::system_error( ev, ecat ) {}
    SystemError( int ev, std::error_category const & ecat, std::string const & what ) : Error(), std::system_error( ev, ecat, what ) {}
    SystemError( int ev, std::error_category const & ecat, char const * what ) : Error(), std::system_error( ev, ecat, what ) {}

    virtual const char * what() const VULKAN_HPP_NOEXCEPT
    {
      return std::system_error::what();
    }
  };

  VULKAN_HPP_INLINE const std::error_category & errorCategory() VULKAN_HPP_NOEXCEPT
  {
    static ErrorCategoryImpl instance;
    return instance;
  }

  VULKAN_HPP_INLINE std::error_code make_error_code( Result e ) VULKAN_HPP_NOEXCEPT
  {
    return std::error_code( static_cast<int>( e ), errorCategory() );
  }

  VULKAN_HPP_INLINE std::error_condition make_error_condition( Result e ) VULKAN_HPP_NOEXCEPT
  {
    return std::error_condition( static_cast<int>( e ), errorCategory() );
  }
)"};

static constexpr char const *RES_RESULT_VALUE{R"(
  template <typename T>
  void ignore( T const & ) VULKAN_HPP_NOEXCEPT
  {
  }

  template <typename T>
  struct ResultValue
  {
#ifdef VULKAN_HPP_HAS_NOEXCEPT
    ResultValue( Result r, T & v ) VULKAN_HPP_NOEXCEPT( VULKAN_HPP_NOEXCEPT( T( v ) ) )
#else
    ResultValue( Result r, T & v )
#endif
      : result( r ), value( v )
    {
    }

#ifdef VULKAN_HPP_HAS_NOEXCEPT
    ResultValue( Result r, T && v ) VULKAN_HPP_NOEXCEPT( VULKAN_HPP_NOEXCEPT( T( std::move( v ) ) ) )
#else
    ResultValue( Result r, T && v )
#endif
      : result( r ), value( std::move( v ) )
    {
    }

    Result result;
    T      value;

    operator std::tuple<Result &, T &>() VULKAN_HPP_NOEXCEPT
    {
      return std::tuple<Result &, T &>( result, value );
    }
  };
/*
#if !defined( VULKAN_HPP_NO_SMART_HANDLE )
  template <typename Type, typename Dispatch>
  struct ResultValue<UniqueHandle<Type, Dispatch>>
  {
#  ifdef VULKAN_HPP_HAS_NOEXCEPT
    ResultValue( Result r, UniqueHandle<Type, Dispatch> && v ) VULKAN_HPP_NOEXCEPT
#  else
    ResultValue( Result r, UniqueHandle<Type, Dispatch> && v )
#  endif
      : result( r )
      , value( std::move( v ) )
    {
    }

    std::tuple<Result, UniqueHandle<Type, Dispatch>> asTuple()
    {
      return std::make_tuple( result, std::move( value ) );
    }

    Result                       result;
    UniqueHandle<Type, Dispatch> value;
  };

  template <typename Type, typename Dispatch>
  struct ResultValue<std::vector<UniqueHandle<Type, Dispatch>>>
  {
#  ifdef VULKAN_HPP_HAS_NOEXCEPT
    ResultValue( Result r, std::vector<UniqueHandle<Type, Dispatch>> && v ) VULKAN_HPP_NOEXCEPT
#  else
    ResultValue( Result r, std::vector<UniqueHandle<Type, Dispatch>> && v )
#  endif
      : result( r )
      , value( std::move( v ) )
    {
    }

    std::tuple<Result, std::vector<UniqueHandle<Type, Dispatch>>> asTuple()
    {
      return std::make_tuple( result, std::move( value ) );
    }

    Result                                    result;
    std::vector<UniqueHandle<Type, Dispatch>> value;
  };
#endif
*/
  template <typename T>
  struct ResultValueType
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    typedef ResultValue<T> type;
#else
    typedef T    type;
#endif
  };

  template <>
  struct ResultValueType<void>
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    typedef Result type;
#else
    typedef void type;
#endif
  };

  VULKAN_HPP_INLINE typename ResultValueType<void>::type createResultValueType( Result result )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    return result;
#else
    ignore( result );
#endif
  }

  template <typename T>
  VULKAN_HPP_INLINE typename ResultValueType<T>::type createResultValueType( Result result, T & data )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    return ResultValue<T>( result, data );
#else
    ignore( result );
    return data;
#endif
  }

  template <typename T>
  VULKAN_HPP_INLINE typename ResultValueType<T>::type createResultValueType( Result result, T && data )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    return ResultValue<T>( result, std::move( data ) );
#else
    ignore( result );
    return std::move( data );
#endif
  }

  VULKAN_HPP_INLINE void resultCheck( Result result, char const * message )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    ignore( result );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    ignore( message );
    VULKAN_HPP_ASSERT_ON_RESULT( result == Result::eSuccess );
#else
    if ( result != Result::eSuccess )
    {
      throwResultException( result, message );
    }
#endif
  }

  VULKAN_HPP_INLINE void resultCheck( Result result, char const * message, std::initializer_list<Result> successCodes )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    ignore( result );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    ignore( message );
    ignore( successCodes );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    VULKAN_HPP_ASSERT_ON_RESULT( std::find( successCodes.begin(), successCodes.end(), result ) != successCodes.end() );
#else
    if ( std::find( successCodes.begin(), successCodes.end(), result ) == successCodes.end() )
    {
      throwResultException( result, message );
    }
#endif
  }
)"};

static constexpr char const *RES_ARRAY_PROXY{R"(
  template <typename T>
  class ArrayProxy
  {
  public:
    VULKAN_HPP_CONSTEXPR ArrayProxy() VULKAN_HPP_NOEXCEPT
      : m_count( 0 )
      , m_ptr( nullptr )
    {}

    VULKAN_HPP_CONSTEXPR ArrayProxy( std::nullptr_t ) VULKAN_HPP_NOEXCEPT
      : m_count( 0 )
      , m_ptr( nullptr )
    {}

    ArrayProxy( T & value ) VULKAN_HPP_NOEXCEPT
      : m_count( 1 )
      , m_ptr( &value )
    {}

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxy( typename std::remove_const<T>::type & value ) VULKAN_HPP_NOEXCEPT
      : m_count( 1 )
      , m_ptr( &value )
    {}

    ArrayProxy( uint32_t count, T * ptr ) VULKAN_HPP_NOEXCEPT
      : m_count( count )
      , m_ptr( ptr )
    {}

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxy( uint32_t count, typename std::remove_const<T>::type * ptr ) VULKAN_HPP_NOEXCEPT
      : m_count( count )
      , m_ptr( ptr )
    {}

#  if __GNUC__ >= 9
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Winit-list-lifetime"
#  endif

    ArrayProxy( std::initializer_list<T> const & list ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( list.size() ) )
      , m_ptr( list.begin() )
    {}

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxy( std::initializer_list<typename std::remove_const<T>::type> const & list ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( list.size() ) )
      , m_ptr( list.begin() )
    {}

    ArrayProxy( std::initializer_list<T> & list ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( list.size() ) )
      , m_ptr( list.begin() )
    {}

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxy( std::initializer_list<typename std::remove_const<T>::type> & list ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( list.size() ) )
      , m_ptr( list.begin() )
    {}

#  if __GNUC__ >= 9
#    pragma GCC diagnostic pop
#  endif

    // Any type with a .data() return type implicitly convertible to T*, and a .size() return type implicitly
    // convertible to size_t. The const version can capture temporaries, with lifetime ending at end of statement.
    template <typename V,
              typename std::enable_if<
                std::is_convertible<decltype( std::declval<V>().data() ), T *>::value &&
                std::is_convertible<decltype( std::declval<V>().size() ), std::size_t>::value>::type * = nullptr>
    ArrayProxy( V const & v ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( v.size() ) )
      , m_ptr( v.data() )
    {}

    template <typename V,
              typename std::enable_if<
                std::is_convertible<decltype( std::declval<V>().data() ), T *>::value &&
                std::is_convertible<decltype( std::declval<V>().size() ), std::size_t>::value>::type * = nullptr>
    ArrayProxy( V & v ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( v.size() ) )
      , m_ptr( v.data() )
    {}

    const T * begin() const VULKAN_HPP_NOEXCEPT
    {
      return m_ptr;
    }

    const T * end() const VULKAN_HPP_NOEXCEPT
    {
      return m_ptr + m_count;
    }

    const T & front() const VULKAN_HPP_NOEXCEPT
    {
      VULKAN_HPP_ASSERT( m_count && m_ptr );
      return *m_ptr;
    }

    const T & back() const VULKAN_HPP_NOEXCEPT
    {
      VULKAN_HPP_ASSERT( m_count && m_ptr );
      return *( m_ptr + m_count - 1 );
    }

    bool empty() const VULKAN_HPP_NOEXCEPT
    {
      return ( m_count == 0 );
    }

    uint32_t size() const VULKAN_HPP_NOEXCEPT
    {
      return m_count;
    }

    T * data() const VULKAN_HPP_NOEXCEPT
    {
      return m_ptr;
    }

  private:
    uint32_t m_count;
    T *      m_ptr;
  };

  template <typename T>
  class ArrayProxyNoTemporaries
  {
  public:
    VULKAN_HPP_CONSTEXPR ArrayProxyNoTemporaries() VULKAN_HPP_NOEXCEPT
      : m_count( 0 )
      , m_ptr( nullptr )
    {}

    VULKAN_HPP_CONSTEXPR ArrayProxyNoTemporaries( std::nullptr_t ) VULKAN_HPP_NOEXCEPT
      : m_count( 0 )
      , m_ptr( nullptr )
    {}

    ArrayProxyNoTemporaries( T & value ) VULKAN_HPP_NOEXCEPT
      : m_count( 1 )
      , m_ptr( &value )
    {}

    template <typename V>
    ArrayProxyNoTemporaries( V && value ) = delete;

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxyNoTemporaries( typename std::remove_const<T>::type & value ) VULKAN_HPP_NOEXCEPT
      : m_count( 1 )
      , m_ptr( &value )
    {}

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxyNoTemporaries( typename std::remove_const<T>::type && value ) = delete;

    ArrayProxyNoTemporaries( uint32_t count, T * ptr ) VULKAN_HPP_NOEXCEPT
      : m_count( count )
      , m_ptr( ptr )
    {}

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxyNoTemporaries( uint32_t count, typename std::remove_const<T>::type * ptr ) VULKAN_HPP_NOEXCEPT
      : m_count( count )
      , m_ptr( ptr )
    {}

    ArrayProxyNoTemporaries( std::initializer_list<T> const & list ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( list.size() ) )
      , m_ptr( list.begin() )
    {}

    ArrayProxyNoTemporaries( std::initializer_list<T> const && list ) = delete;

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxyNoTemporaries( std::initializer_list<typename std::remove_const<T>::type> const & list )
      VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( list.size() ) )
      , m_ptr( list.begin() )
    {}

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxyNoTemporaries( std::initializer_list<typename std::remove_const<T>::type> const && list ) = delete;

    ArrayProxyNoTemporaries( std::initializer_list<T> & list ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( list.size() ) )
      , m_ptr( list.begin() )
    {}

    ArrayProxyNoTemporaries( std::initializer_list<T> && list ) = delete;

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxyNoTemporaries( std::initializer_list<typename std::remove_const<T>::type> & list ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( list.size() ) )
      , m_ptr( list.begin() )
    {}

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxyNoTemporaries( std::initializer_list<typename std::remove_const<T>::type> && list ) = delete;

    // Any type with a .data() return type implicitly convertible to T*, and a // .size() return type implicitly
    // convertible to size_t.
    template <typename V,
              typename std::enable_if<
                std::is_convertible<decltype( std::declval<V>().data() ), T *>::value &&
                std::is_convertible<decltype( std::declval<V>().size() ), std::size_t>::value>::type * = nullptr>
    ArrayProxyNoTemporaries( V & v ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( v.size() ) )
      , m_ptr( v.data() )
    {}

    const T * begin() const VULKAN_HPP_NOEXCEPT
    {
      return m_ptr;
    }

    const T * end() const VULKAN_HPP_NOEXCEPT
    {
      return m_ptr + m_count;
    }

    const T & front() const VULKAN_HPP_NOEXCEPT
    {
      VULKAN_HPP_ASSERT( m_count && m_ptr );
      return *m_ptr;
    }

    const T & back() const VULKAN_HPP_NOEXCEPT
    {
      VULKAN_HPP_ASSERT( m_count && m_ptr );
      return *( m_ptr + m_count - 1 );
    }

    bool empty() const VULKAN_HPP_NOEXCEPT
    {
      return ( m_count == 0 );
    }

    uint32_t size() const VULKAN_HPP_NOEXCEPT
    {
      return m_count;
    }

    T * data() const VULKAN_HPP_NOEXCEPT
    {
      return m_ptr;
    }

  private:
    uint32_t m_count;
    T *      m_ptr;
  };
)"};

static constexpr char const *RES_BASE_TYPES{R"(
  //==================
  //=== BASE TYPEs ===
  //==================

  using Bool32          = uint32_t;
  using DeviceAddress   = uint64_t;
  using DeviceSize      = uint64_t;
  using RemoteAddressNV = void *;
  using SampleMask      = uint32_t;
)"};

static constexpr char const *RES_FLAGS{R"(
template <typename FlagBitsType>
struct FlagTraits
{
    enum
    {
        allFlags = 0
    };
};

template <typename BitType>
class Flags {
public:
    using MaskType = typename std::underlying_type<BitType>::type;

    // constructors
    VULKAN_HPP_CONSTEXPR Flags() VULKAN_HPP_NOEXCEPT : m_mask( 0 ) {}

    VULKAN_HPP_CONSTEXPR Flags( BitType bit ) VULKAN_HPP_NOEXCEPT : m_mask( static_cast<MaskType>( bit ) ) {}

    VULKAN_HPP_CONSTEXPR Flags( Flags<BitType> const & rhs ) VULKAN_HPP_NOEXCEPT = default;

    VULKAN_HPP_CONSTEXPR explicit Flags( MaskType flags ) VULKAN_HPP_NOEXCEPT : m_mask( flags ) {}

    // relational operators
#if defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )
    auto operator<=>( Flags<BitType> const & ) const = default;
#else
    VULKAN_HPP_CONSTEXPR bool operator<( Flags<BitType> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return m_mask < rhs.m_mask;
    }

    VULKAN_HPP_CONSTEXPR bool operator<=( Flags<BitType> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return m_mask <= rhs.m_mask;
    }

    VULKAN_HPP_CONSTEXPR bool operator>( Flags<BitType> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return m_mask > rhs.m_mask;
    }

    VULKAN_HPP_CONSTEXPR bool operator>=( Flags<BitType> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return m_mask >= rhs.m_mask;
    }

    VULKAN_HPP_CONSTEXPR bool operator==( Flags<BitType> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return m_mask == rhs.m_mask;
    }

    VULKAN_HPP_CONSTEXPR bool operator!=( Flags<BitType> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return m_mask != rhs.m_mask;
    }
#endif

    // logical operator
    VULKAN_HPP_CONSTEXPR bool operator!() const VULKAN_HPP_NOEXCEPT
    {
      return !m_mask;
    }

    // bitwise operators
    VULKAN_HPP_CONSTEXPR Flags<BitType> operator&( Flags<BitType> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return Flags<BitType>( m_mask & rhs.m_mask );
    }

    VULKAN_HPP_CONSTEXPR Flags<BitType> operator|( Flags<BitType> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return Flags<BitType>( m_mask | rhs.m_mask );
    }

    VULKAN_HPP_CONSTEXPR Flags<BitType> operator^( Flags<BitType> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return Flags<BitType>( m_mask ^ rhs.m_mask );
    }

    VULKAN_HPP_CONSTEXPR Flags<BitType> operator~() const VULKAN_HPP_NOEXCEPT
    {
      return Flags<BitType>( m_mask ^ FlagTraits<BitType>::allFlags );
    }

    // assignment operators
    VULKAN_HPP_CONSTEXPR_14 Flags<BitType> & operator=( Flags<BitType> const & rhs ) VULKAN_HPP_NOEXCEPT = default;

    VULKAN_HPP_CONSTEXPR_14 Flags<BitType> & operator|=( Flags<BitType> const & rhs ) VULKAN_HPP_NOEXCEPT
    {
      m_mask |= rhs.m_mask;
      return *this;
    }

    VULKAN_HPP_CONSTEXPR_14 Flags<BitType> & operator&=( Flags<BitType> const & rhs ) VULKAN_HPP_NOEXCEPT
    {
      m_mask &= rhs.m_mask;
      return *this;
    }

    VULKAN_HPP_CONSTEXPR_14 Flags<BitType> & operator^=( Flags<BitType> const & rhs ) VULKAN_HPP_NOEXCEPT
    {
      m_mask ^= rhs.m_mask;
      return *this;
    }

    // cast operators
    explicit VULKAN_HPP_CONSTEXPR operator bool() const VULKAN_HPP_NOEXCEPT
    {
      return !!m_mask;
    }

    explicit VULKAN_HPP_CONSTEXPR operator MaskType() const VULKAN_HPP_NOEXCEPT
    {
      return m_mask;
    }

#if defined( VULKAN_HPP_FLAGS_MASK_TYPE_AS_PUBLIC )
public:
#else
private:
#endif
    MaskType m_mask;
};
)"};

static constexpr char const *RES_OPTIONAL{R"(
  template <typename RefType>
  class Optional {
  public:
    Optional( RefType & reference ) {NOEXCEPT}
    {
      m_ptr = &reference;
    }
    Optional( RefType * ptr ) {NOEXCEPT}
    {
      m_ptr = ptr;
    }
    Optional( std::nullptr_t ) {NOEXCEPT}
    {
      m_ptr = nullptr;
    }

    operator RefType *() const {NOEXCEPT}
    {
      return m_ptr;
    }
    RefType const * operator->() const {NOEXCEPT}
    {
      return m_ptr;
    }
    explicit operator bool() const {NOEXCEPT}
    {
      return !!m_ptr;
    }

  private:
    RefType * m_ptr;
  };
)"};

static constexpr char const *RES_RAII{R"(
    template <class T, class U = T>
    VULKAN_HPP_CONSTEXPR_14 {INLINE} T exchange( T & obj, U && newValue ) {
#  if ( 14 <= VULKAN_HPP_CPP_VERSION )
      return std::exchange<T>( obj, std::forward<U>( newValue ) );
#  else
      T oldValue = std::move( obj );
      obj        = std::forward<U>( newValue );
      return oldValue;
#  endif
    }
)"};

// static constexpr char const *RES_FLAGS{R"()"};

template <typename T> class EnumFlag {
    using Type = typename std::underlying_type<T>::type;
    Type m_flags;

  public:
    EnumFlag() = default;

    EnumFlag(const T &flags) : m_flags(static_cast<Type>(flags)) {}

    bool operator&(const T &rhs) const {
        return (m_flags & static_cast<Type>(rhs)) != 0;
    }

    operator const T() const { return m_flags; }

    EnumFlag<T> &operator|=(const T &b) {
        m_flags |= static_cast<Type>(b);
        return *this;
    }

    EnumFlag<T> &operator&=(const T &b) {
        m_flags &= static_cast<Type>(b);
        return *this;
    }

    const T operator~() const { return static_cast<T>(~m_flags); }
};

template <template <class...> class TContainer, class T, class A, class E>
static bool isInContainter(const TContainer<T, A> &array, E entry) {
    return std::find(std::begin(array), std::end(array), entry) !=
           std::end(array);
}

using namespace tinyxml2;

static String convertName(const std::string &name, const std::string &cls) {
    String out{name, false};
    out = std::regex_replace(out, std::regex(cls, std::regex_constants::icase),
                             "");
    return out; // filtered prototype name
}

class Generator {

  public:
    struct Macro {
        std::string value;
        std::string define;
        bool usesDefine;

        std::string get() const { return usesDefine ? define : value; }
    };

    struct Config {
        struct {
            Macro mNamespace;
            Macro mNamespaceRAII;
            Macro mNamespaceSTD;
            Macro mConstexpr;
            Macro mInline;
            Macro mNoexcept;
            Macro mExplicit;
            Macro mDispatch;
            Macro mDispatchType;
        } macro;
        struct {
            bool cppModules;
            bool structNoinit;
            bool vulkanCommands;
            bool dispatchParam;
            bool dispatchLoaderStatic;
            bool useStaticCommands; // move
            bool allocatorParam;
            bool smartHandles;
            bool exceptions;
            bool resultValueType;
        } gen;
        struct {
            bool methodTags;
        } dbg;
        std::string fileProtect;
        std::string loaderClassName;
    };

    struct ExtensionData;

    struct BaseType {
        String name = {""};
        ExtensionData *ext = {};
        std::unordered_set<BaseType *> dependencies;
        std::unordered_set<BaseType *> subscribers;
        bool forceRequired = {};

        void setUnsuppored() {
            supported = false;
        }

        bool isEnabled() const {
            return enabled && supported;
        }

        bool isSuppored() const {
            return supported;
        }

        bool isRequired() const {
            return !subscribers.empty() || forceRequired;
        }

        bool canGenerate() const {
            return supported && (enabled || isRequired());
        }

        void setEnabled(bool value) {
            if (enabled == value || !supported) {
                return;
            }
            enabled = value;
            if (enabled) {
                // std::cout << "> enable: " << name << std::endl;
                for (auto &d : dependencies) {
                    if (!subscribers.contains(d)) {
                        d->subscribe(this);
                    }
                }
            }
            else {
                // std::cout << "> disable: " << name << std::endl;
                for (auto &d : dependencies) {
                    if (!subscribers.contains(d)) {
                        d->unsubscribe(this);
                    }
                }
            }
        }

        void subscribe(BaseType *s) {
            if (!subscribers.contains(s)) {
                bool empty = subscribers.empty();
                subscribers.emplace(s);
                // std::cout << "  subscribed to: " << name << std::endl;
                if (empty) {
                    setEnabled(true);
                }
            }
        }

        void unsubscribe(BaseType *s) {
            auto it = subscribers.find(s);
            if (it != subscribers.end()) {
                subscribers.erase(it);
                // std::cout << "  unsubscribed to: " << name << std::endl;
                if (subscribers.empty()) {
                    setEnabled(false);
                }
            }
        }

    protected:
        bool enabled = false;
        bool supported = true;
    };    

    struct PlatformData {
        std::string name;
        std::string_view protect;
        bool enabled = {};

        PlatformData(const std::string &name, std::string_view protect, bool enabled) :
            name(name), protect(protect), enabled(enabled)
        {
        }

        void setEnabled(bool value) {
            enabled = value;
        }

        bool isEnabled() const {
            return enabled;
        }

        bool isRequired() const {
            return enabled;
        }
    };
    using Platforms = std::map<std::string, PlatformData>;
    Platforms platforms; // maps platform name to protect (#if defined PROTECT)

    using Tags = std::unordered_set<std::string>;
    Tags tags; // list of tags from <tags>

    struct CommandData;
    struct StructData;

    struct ExtensionData {
        std::string name;
        std::string protect;
        Platforms::pointer platform;
        bool supported = true;
        bool enabled = {};

        ExtensionData(const std::string &name, std::string protect, Platforms::pointer platform, bool supported, bool enabled) :
            name(name), platform(platform), protect(protect), supported(supported), enabled(enabled)
        {
        }

        std::vector<CommandData *> commands;
        std::vector<BaseType *> types;

        void setEnabled(bool value) {
            enabled = value;
        }

        bool isEnabled() const {
            return enabled;
        }

        bool isRequired() const {
            return enabled;
        }

    };

    using Extensions = std::map<std::string, ExtensionData>;
    Extensions extensions; // maps extension to protect as reference

    using GenFunction = std::function<void(XMLNode *)>;
    using OrderPair = std::pair<std::string, GenFunction>;

    using Variables = std::vector<std::shared_ptr<VariableData>>;

    std::unordered_map<Namespace, Macro *> namespaces;

    void bindVars(Variables &vars) const;

    enum class PFNReturnCategory {
        OTHER,
        VOID,
        VK_RESULT
    };

    enum class ArraySizeArgument { INVALID, COUNT, SIZE, CONST_COUNT };

    enum class MemberNameCategory {
        UNKNOWN,
        GET,
        ALLOCATE,
        ACQUIRE,
        CREATE,
        ENUMERATE,
        WRITE,
        DESTROY,
        FREE
    };

    enum class HandleCreationCategory { NONE, ALLOCATE, CREATE };

    enum class CommandFlags : uint8_t {
        NONE,
        ALIAS = 1,
        INDIRECT = 2,
        RAII_ONLY = 4
    };

    struct HandleData;

    struct CommandData : public BaseType {
        std::string type; // return type
        Variables params; // list of arguments
        std::vector<std::string> successCodes;
        MemberNameCategory nameCat;
        PFNReturnCategory pfnReturn;
        EnumFlag<CommandFlags> flags{};

        void setFlagBit(CommandFlags bit, bool enabled) {
            if (enabled) {
                flags |= bit;
            } else {
                flags &= ~EnumFlag<CommandFlags>{bit};
                ;
            }
        }

        bool isAlias() const {
            bool r = flags & CommandFlags::ALIAS;
            return r;
        }

        bool getsObject() const {
            switch (nameCat) {
            case MemberNameCategory::ACQUIRE:
            case MemberNameCategory::GET:
                return true;
            default:
                return false;
            }
        }

        bool createsObject() const {
            switch (nameCat) {
            case MemberNameCategory::ALLOCATE:
            case MemberNameCategory::CREATE:
                return true;
            default:
                return false;
            }
        }

        bool destroysObject() const {
            switch (nameCat) {
            case MemberNameCategory::DESTROY:
            case MemberNameCategory::FREE:
                return true;
            default:
                return false;
            }
        }

        bool isIndirectCandidate(const std::string_view &type) const {
            try {
                if (getsObject() || createsObject()) {
                    const auto &var = getLastPointerVar();
                    return var->original.type() != type;
                } else if (destroysObject()) {
                    const auto &var = getLastHandleVar();
                    return var->original.type() != type;
                }
            } catch (std::runtime_error) {
            }
            return true;
        }

        void setName(const Generator &gen, const std::string &name) {
            this->name.convert(name);
            pfnReturn = gen.evaluatePFNReturn(type);
            gen.evalCommand(*this);
        }

        bool containsPointerVariable() {
            for (auto it = params.rbegin(); it != params.rend(); it++) {
                if (it->get()->original.isPointer()) {
                    return true;
                }
            }
            return false;
        }

        std::shared_ptr<VariableData> getLastVar() {
            return *std::prev(params.end());
        }

        std::shared_ptr<VariableData> getLastVisibleVar() {
            for (auto it = params.rbegin(); it != params.rend(); it++) {
                if (!it->get()->getIgnoreFlag()) {
                    return *it;
                }
            }
            throw std::runtime_error("can't get param (last visible)");
        }

        std::shared_ptr<VariableData> getLastPointerVar() const {
            for (auto it = params.rbegin(); it != params.rend(); it++) {
                if (it->get()->original.isPointer()) {
                    return *it;
                }
            }
            throw std::runtime_error("can't get param (last pointer)");
        }

        std::shared_ptr<VariableData> getLastHandleVar() const {
            for (auto it = params.rbegin(); it != params.rend(); it++) {
                if (it->get()->isHandle()) {
                    return *it;
                }
            }
            throw std::runtime_error("can't get param (last handle)");
        }
    };

    // holds information about class member (function)
    struct ClassCommandData {

        const Generator *gen = {};
        const CommandData *src = {};
        HandleData *cls = {};
        String name = {""};
        bool raiiOnly = {};

        ClassCommandData(const Generator *gen, HandleData *cls,
                         const CommandData &o)
            : gen(gen), cls(cls), src(&o),
              name(convertName(o.name.original, cls->name), false)
        {
            name.original = o.name.original;
        }

        ClassCommandData &operator=(ClassCommandData &o) noexcept = default;

        bool valid() const { return !name.empty(); }
    };

    struct EnumValue : public BaseType {
        bool isAlias = {};
        bool supported = {};

        EnumValue(const String &name, bool isAlias = false, bool enabled = true) : isAlias(isAlias) {
            this->name = name;
            this->enabled = true;
        }
    };

    struct EnumData : public BaseType {
        std::vector<String> aliases;
        std::vector<EnumValue> members;
        std::string flagbits;
        bool isBitmask = {};
        // bool isFlag = {};

        EnumData(std::string name) { this->name = String(name, true); }

        bool containsValue(const std::string &value) const;
    };

    Config cfg;
    bool loaded;
    XMLDocument doc;
    XMLElement *root;

    std::string headerVersion;

    bool dispatchLoaderBaseGenerated;

    std::vector<const EnumValue *> errorClasses;

    std::map<std::string, CommandData> commands;
    std::vector<CommandData*> staticCommands;

    std::string outputFuncs;
    std::string outputFuncsRAII;

    std::string outputFilePath;

    struct StructData : BaseType {
        enum VkStructType { VK_STRUCT, VK_UNION } type;
        // XMLNode *node;
        std::string structTypeValue;
        std::vector<String> aliases;
        Variables members;

        std::string getType() const {
            return type == StructData::VK_STRUCT ? "struct" : "union";
        }
    };

    std::map<std::string, StructData> structs;
    std::list<StructData *> structBuffer;

    bool isStructOrUnion(const std::string &name) const;

    std::map<std::string, EnumData> enums;
    std::map<std::string, EnumData *> enumMap;

    struct GenOutputClass {
        std::string sPublic;
        std::string sPrivate;
        std::string sProtected;
        std::string inherits;
    };

    class UnorderedOutput {
        const Generator &gen;
        std::unordered_map<std::string, std::string> output;

      public:
        UnorderedOutput(const Generator &gen) : gen(gen) {}

        std::string get() const {
            std::string out;
            return out;
        }
    };

    template <class... Args>
    std::string format(const std::string &format, const Args... args) const;

    bool defaultWhitelistOption = true;

    void parsePlatforms(XMLNode *node);

    void parseFeature(XMLNode *node);

    void parseExtensions(XMLNode *node);

    void parseTags(XMLNode *node);

    // tries to match str in extensions, if found returns pointer to protect,
    // otherwise nullptr    
//    ExtensionData *findExtensionProtect(const std::string &str);

//    bool idLookup(const std::string &name, std::string_view &protect) const;

//    // #if defined encapsulation
//    void withProtect(std::string &output, const char *protect,
//                     std::function<void(std::string &)> function) const;

    // void genOptional(const std::string &name, std::function<void()>
    // function);

//    std::string genOptional(const std::string &name,
//                            std::function<void(std::string &)> function) const;

    std::string genOptional(const BaseType &type,
                            std::function<void(std::string &)> function) const;

    std::string strRemoveTag(std::string &str) const;

    std::string strWithoutTag(const std::string &str) const;

    std::pair<std::string, std::string> snakeToCamelPair(std::string str) const;

    std::string snakeToCamel(const std::string &str) const;

    std::string enumConvertCamel(const std::string &enumName, std::string value,
                                 bool isBitmask = false) const;

    std::string genNamespaceMacro(const Macro &m);

    std::string generateHeader();

    void generateFiles(std::filesystem::path path);

    std::string generateMainFile();

    Variables parseStructMembers(XMLElement *node, std::string &structType,
                                 std::string &structTypeValue);

    void parseEnumExtend(XMLElement &node, ExtensionData *ext);

    std::string generateEnum(const EnumData &data, const std::string &name);

    std::string generateEnums();

    std::string genFlagTraits(const EnumData &data, std::string name);

    std::string generateDispatch();

    std::string generateErrorClasses();

    std::string generateDispatchLoaderBase();

    std::string generateDispatchLoaderStatic();

    bool useDispatchLoader() const {
        const auto &cfg = getConfig();
        return cfg.gen.dispatchLoaderStatic && !cfg.gen.useStaticCommands;
    }

    std::string getDispatchArgument(bool assignment) const {
        if (!cfg.gen.dispatchParam) {
            return "";
        }
        std::string out =
            format("::{NAMESPACE}::DispatchLoaderStatic const &d");
        if (assignment) {
            out += " " + cfg.macro.mDispatch.get();
        }
        return out;
    }

    std::string getDispatchType() const {
        if (cfg.macro.mDispatchType.usesDefine) {
            return cfg.macro.mDispatchType.define;
        }
        return format(cfg.macro.mDispatchType.value);
    }

    Argument getDispatchArgument() const {
        if (!cfg.gen.dispatchParam) {
            return Argument("", "");
        }
        std::string assignment = cfg.macro.mDispatch.get();
        if (!assignment.empty()) {
            assignment = " " + assignment;
        }
        auto type = format("::{NAMESPACE}::DispatchLoaderStatic const &");
        return Argument(type, "d", assignment);
    }

    std::string getDispatchCall(const std::string &var = "d.") const {
        if (cfg.gen.dispatchParam) {
            return var;
        }
        return "::";
    }

    struct MemberContext : CommandData {
        using Var = const std::shared_ptr<VariableData>;

        const Generator &gen;
        HandleData *cls;
        Namespace ns;
        std::string pfnSourceOverride;
        std::string pfnNameOverride;
        [[deprecated]] bool lastHandleVariant = {};
        bool raiiOnly = {};
        bool useThis = {};
        bool isStatic = {};
        bool inUnique = {};
        bool constructor = {};
        bool generateInline = {};
        bool disableSubstitution = {};

        MemberContext(const ClassCommandData &m, Namespace ns,
                      bool constructor = false)
            : gen(*m.gen), ns(ns), constructor(constructor),
              CommandData(*m.src) {
            name = m.name;
            cls = m.cls;
            raiiOnly = m.raiiOnly;

            const auto &p = m.src->params;
            params.clear();
            params.reserve(p.size());
            std::transform(p.begin(), p.end(), std::back_inserter(params),
                           [](const std::shared_ptr<VariableData> &var) {
                               return std::make_shared<VariableData>(
                                   *var.get());
                           });

            gen.bindVars(params);
        }

        MemberContext(const MemberContext &o) : gen(o.gen), CommandData(o) {
            cls = o.cls;
            ns = o.ns;
            pfnSourceOverride = o.pfnSourceOverride;
            pfnNameOverride = o.pfnNameOverride;
            raiiOnly = o.raiiOnly;
            constructor = o.constructor;
            useThis = o.useThis;
            isStatic = o.isStatic;
            inUnique = o.inUnique;
            generateInline = o.generateInline;
            disableSubstitution = o.disableSubstitution;

            const auto &p = o.params;
            params.clear();
            params.reserve(p.size());
            std::transform(p.begin(), p.end(), std::back_inserter(params),
                           [](const std::shared_ptr<VariableData> &var) {
                               return std::make_shared<VariableData>(
                                   *var.get());
                           });
            gen.bindVars(params);
        }

        bool isRaiiOnly() const { return raiiOnly; }

        bool isIndirect() const { return cls->isSubclass; }

        std::string createProtoArguments(bool useOriginal = false, bool declaration = false) const {
            return createArguments(
                filterProto,
                [&](Var &v) {
                    if (useOriginal) {
                        return v->originalToString();
                    }
                    if (declaration) {
                        return v->toStringWithAssignment();
                    }
                    return v->toString();
                },
                true);
        }

        std::string createPFNArguments(bool useOriginal = false) const {
            return createArguments(
                filterPFN, [&](Var &v) { return v->toArgument(useOriginal); });
        }

        std::string createPassArguments(bool hasAllocVar) const {
            return createArguments(filterPass,
                                   [&](Var &v) { return v->identifier(); },
                                   false);
        }

        Variables getFilteredProtoVars() const {
            Variables out;
            for (const auto &p : params) {
                if (filterProto(p, p->original.type() == cls->name.original)) {
                    out.push_back(p);
                }
            }
            return out;
        }

      private:
        static bool filterProto(Var &v, bool same) {
            if (v->getIgnoreFlag()) {
                return false;
            }
            return v->hasLengthVar() || !same;
        }

        static bool filterPass(Var &v, bool same) {
            return !v->getIgnoreFlag();
        }

        static bool filterPFN(Var &v, bool same) { return !v->getIgnorePFN(); }

        std::string createArguments(std::function<bool(Var &, bool)> filter,
                                    std::function<std::string(Var &)> function,
                                    bool proto = false) const {
            std::string out;
            for (const auto &p : params) {
                bool sameType = p->original.type() == cls->name.original && !p->original.type().empty();

                if (ns == Namespace::RAII && cls->isSubclass && !constructor &&
                    p->original.type() == cls->superclass.original) {
                    continue;
                }

                if (!filter(p, sameType)) {
                    continue;
                }
                if (!disableSubstitution && !proto && p->type() == "AllocationCallbacks" &&
                    !gen.getConfig().gen.allocatorParam) {
                    out += "nullptr";
                } else if (!p->hasLengthVar() && sameType) {
                    if (!useThis && p->original.isPointer()) {
                        out += "&";
                    } else if (useThis) {
                        out += "*";
                    }
                    out += useThis ? "this" : cls->vkhandle;
                } else {
                    bool match = false;
                    if (inUnique && !proto && !disableSubstitution && !p->hasLengthVar()) {
                        for (const auto &v : cls->uniqueVars) {
                            if (p->type() == v.type()) {
                                out += matchTypePointers(v.suffix(), p->original.suffix());
                                out += v.identifier();
                                match = true;
                                break;
                            }
                        }
                    }
                    if (!match) {
                        out += function(p);
                    }
                }
                out += ", ";
            }
            strStripSuffix(out, ", ");
            return out;
        }
    };

    struct HandleData : public BaseType {

        String superclass;
        std::string vkhandle;
        std::string ownerhandle;
        std::string alias;
        HandleData *parent;
        HandleCreationCategory creationCat;

        std::optional<ClassCommandData> getAddrCmd;
        std::vector<ClassCommandData> members;
        std::vector<ClassCommandData> ctorCmds;
        std::vector<CommandData *> dtorCmds;
        std::vector<ClassCommandData> vectorCmds;

        std::vector<MemberContext> generated;

        std::vector<VariableData> uniqueVars;
        std::vector<VariableData> raiiVars;

        int effectiveMembers;
        bool isSubclass;
        bool vectorVariant;

        // bool uniqueVariant;

        HandleData(const std::string &name, bool isSubclass = false)
            : superclass(""), isSubclass(isSubclass) {
            this->name = String(name, true);
            vkhandle = "m_" + strFirstLower(this->name);
            parent = nullptr;
            effectiveMembers = 0;
            // uniqueVariant = false;
            creationCat = HandleCreationCategory::NONE;


        }

        void clear() {            
            generated.clear();
            uniqueVars.clear();
            raiiVars.clear();
        }

        void init(const Generator &gen,
                       const std::string &loaderClassName);

        void addCommand(const Generator &gen, const CommandData &cmd,
                        bool raiiOnly = false);

        bool hasPFNs() const { return effectiveMembers > 0 && !isSubclass; }

        bool uniqueVariant() const {
            return creationCat != HandleCreationCategory::NONE;
        }
    };

    using Handles = std::map<std::string, HandleData>;
    Handles handles;
    HandleData loader;

    bool isHandle(const std::string &name) const {
        return handles.find(name) != handles.end() || name == loader.name.original;
    }

    HandleData &findHandle(const std::string &name) {
        if (name == loader.name.original) {
            return loader;
        }
        const auto &handle = handles.find(name);
        if (handle == handles.end()) {
            throw std::runtime_error("Handle not found: " + std::string(name));
        }
        return handle->second;
    }

    const HandleData &findHandle(const std::string &name) const {
        if (name == loader.name.original) {
            return loader;
        }
        const auto &handle = handles.find(name);
        if (handle == handles.end()) {
            throw std::runtime_error("Handle not found: " + std::string(name));
        }
        return handle->second;
    }

    CommandData *findCommand(const std::string &name) {
        for (auto &c : commands) {
            if (c.second.name.original == name) {
                return &c.second;
            }
        }
        return nullptr;
    }

    BaseType *findType(const std::string &name) {
        // TODO refactor
        for (auto &c : structs) {
            if (c.second.name.original == name) {
                return &c.second;
            }
        }
        for (auto &c : enumMap) {
            if (c.second->name.original == name) {
                return c.second;
            }
        }
        for (auto &c : handles) {
            if (c.second.name.original == name) {
                return &c.second;
            }
        }
        return nullptr;
    }

    void parseTypes(XMLNode *node);

    void parseEnums(XMLNode *node);

    std::string generateStructDecl(const std::string &name,
                                   const StructData &d);

    std::string generateClassDecl(const HandleData &data, bool allowUnique);

    std::string generateClassString(const std::string &className,
                                    const GenOutputClass &from);

    std::string generateHandles();

    std::string generateStructs();

    std::string generateStruct(const StructData &data);

    std::string generateRAII();

    CommandData parseClassMember(XMLElement *command,
                                 const std::string &className) {
        CommandData m;

        // iterate contents of <command>
        std::string dbg;
        std::string name;
        for (XMLElement &child : Elements(command)) {
            // <proto> section
            dbg += std::string(child.Value()) + "\n";
            if (std::string_view(child.Value()) == "proto") {
                // get <name> field in proto
                XMLElement *nameElement = child.FirstChildElement("name");
                if (nameElement) {
                    name = nameElement->GetText();
                }
                // get <type> field in proto
                XMLElement *typeElement = child.FirstChildElement("type");
                if (typeElement) {
                    m.type = typeElement->GetText();
                }
            }
            // <param> section
            else if (std::string_view(child.Value()) == "param") {
                // parse inside of param
                const std::shared_ptr<XMLVariableParser> &parser =
                    std::make_shared<XMLVariableParser>(&child, *this);
                // add proto data to list
                m.params.push_back(parser);
            }
        }
        if (name.empty()) {
            std::cerr << "Command has no name" << std::endl;
        }

        const char *successcodes = command->Attribute("successcodes");
        if (successcodes) {
            for (const auto &str : split(std::string(successcodes), ",")) {
                m.successCodes.push_back(str);
            }
        }

        bindVars(m.params);
        m.setName(*this, name);
        return m;
    }

    void evalCommand(CommandData &ctx) const;

    static MemberNameCategory evalNameCategory(const std::string &name);

    static bool isTypePointer(const VariableData &m) {
        return strContains(m.suffix(), "*");
    }

    ArraySizeArgument evalArraySizeArgument(const VariableData &m) {
        if (m.identifier().ends_with("Count")) {
            return isTypePointer(m) ? ArraySizeArgument::COUNT
                                    : ArraySizeArgument::CONST_COUNT;
        }
        if (m.identifier().ends_with("Size")) {
            return ArraySizeArgument::SIZE;
        }
        return ArraySizeArgument::INVALID;
    }

    class MemberResolverBase {

      protected:
        MemberContext ctx;
        VariableData resultVar;
        std::string returnType;
        std::string returnValue;
        std::string initializer;
        bool specifierInline;
        bool specifierExplicit;
        bool specifierConst;
        bool disableCheck;
        std::shared_ptr<VariableData> allocatorVar;

        std::string declareReturnVar(const std::string &assignment = "") {
            if (!resultVar.isInvalid()) {
                return "";
            }
            resultVar.setSpecialType(VariableData::TYPE_DEFAULT);
            resultVar.setIdentifier("result");
            resultVar.setFullType("", "Result", "");

            std::string out = resultVar.toString();
            if (!assignment.empty()) {
                out += " = " + assignment;
            }
            return out += ";\n";
        }

        virtual std::string generateMemberBody() = 0;

        std::string castTo(const std::string &type,
                           const std::string &src) const {
            if (type != ctx.type) {
                return "static_cast<" + type + ">(" + src + ")";
            }
            return src;
        }

        bool useDispatchLoader() const {
            return ctx.ns == Namespace::VK && ctx.gen.useDispatchLoader();
        }

        std::string generatePFNcall(bool immediateReturn = false) {
            std::string call = ctx.pfnSourceOverride;
            if (call.empty()) {
                if (ctx.ns == Namespace::RAII) {
                    call += "m_";
                } else {
                    call += ctx.gen.getDispatchCall();
                }
            }
            if (ctx.pfnNameOverride.empty()) {
                call += ctx.name.original;
            } else {
                call += ctx.pfnNameOverride;
            }
            call += "(" + ctx.createPFNArguments() + ")";

            switch (ctx.pfnReturn) {
            case PFNReturnCategory::VK_RESULT:
                call = castTo("Result", call);
                if (!immediateReturn) {
                    return assignToResult(call);
                }
                break;
            case PFNReturnCategory::OTHER:
                call = castTo(returnType, call);
                break;
            case PFNReturnCategory::VOID:
                return call + ";";
            default:
                break;
            }
            if (immediateReturn) {
                call = "return " + call;
            }
            return call + ";";
        }

        std::string assignToResult(const std::string &assignment) {
            if (resultVar.isInvalid()) {
                return declareReturnVar(assignment);
            } else {
                return resultVar.identifier() + " = " + assignment + ";";
            }
        }

        std::string generateReturnValue(const std::string &identifier) {
            if (resultVar.isInvalid() || !usesResultValueType()) {
                return identifier;
            }

            std::string out = "createResultValueType";
            //            if (returnType != "void" && returnType != "Result") {
            //                out += "<" + returnType + ">";
            //            }
            out += "(";
            if (resultVar.identifier() != identifier) {
                out += resultVar.identifier() + ", ";
            }
            out += identifier;
            out += ")";
            return out;
        }

        std::string generateCheck() {
            if (ctx.pfnReturn != PFNReturnCategory::VK_RESULT ||
                resultVar.isInvalid()) {
                return "";
            }
            const auto &macros = ctx.gen.getConfig().macro;
            const auto &ns = ctx.ns == Namespace::VK ? macros.mNamespace
                                                     : macros.mNamespaceRAII;
            std::string message;
            if (ns.usesDefine) {
                message = ns.define + "_STRING \"";
            } else {
                message = "\"" + ns.value;
            }
            if (!ctx.cls->name.empty()) {
                message += "::" + ctx.cls->name;
            }
            message += "::" + ctx.name + "\"";

            std::string codes;
            if (ctx.successCodes.size() > 1) {
                codes += ",\n                { ";
                for (const auto &c : ctx.successCodes) {
                    codes +=
                        "Result::" + ctx.gen.enumConvertCamel("Result", c) +
                        ",\n                  ";
                }
                strStripSuffix(codes, ",\n                  ");
                codes += "}";
            }

            std::string output =
                ctx.gen.format(R"(
    resultCheck({0},
                {1}{2});
)",
                               resultVar.identifier(), message, codes);

            return output;
        }

        bool usesResultValueType() const {
            const auto &cfg = ctx.gen.getConfig();
            if (!cfg.gen.resultValueType) {
                return false;
            }
            return !returnType.empty() && returnType != "Result" &&
                   ctx.pfnReturn == PFNReturnCategory::VK_RESULT;
        }

        std::string generateReturnType() {
            if (usesResultValueType()) {
                return "ResultValueType<" + returnType + ">::type";
            }
            return returnType;
        }

        std::string getSpecifiers(bool decl) {
            std::string output;
            const auto &cfg = ctx.gen.getConfig();
            if (specifierInline && !decl) {
                output += cfg.macro.mInline.get() + " ";
            }
            if (specifierExplicit && decl) {
                output += cfg.macro.mExplicit.get() + " ";
            }
            return output;
        }

        std::string getProto(const std::string &indent, bool declaration) {
            std::string tag = getDbgtag();
            std::string output;
            if (!tag.empty()) {
                output += indent + tag;
            }

            std::string temp;
            for (const auto &p : ctx.params) {
                const std::string &str = p->getTemplate();
                if (!str.empty()) {
                    temp += "typename " + str + ", ";
                }
            }
            strStripSuffix(temp, ", ");
            if (!temp.empty()) {
                output += indent + "template <" + temp + ">\n";
            }

            std::string spec = getSpecifiers(declaration);
            std::string ret = generateReturnType();
            output += indent;
            if (!spec.empty()) {
                output += spec;
            }
            if (!ret.empty()) {
                output += ret + " ";
            }
            if (!declaration && !ctx.isStatic) {
                output += ctx.cls->name + "::";
            }

            output += ctx.name + "(" + createProtoArguments(declaration) + ")";
            if (ctx.constructor && !initializer.empty()) {
                output += initializer;
            }
            if (specifierConst && !ctx.isStatic) {
                output += " const";
            }
            return output;
        }

        std::string getDbgtag();

        std::string generateDeclaration() {
            return ctx.gen.genOptional(ctx, [&](std::string &output) {
                std::string indent = ctx.isStatic ? "  " : "    ";
                output += getProto(indent, true) + ";\n";
            });
        }

        std::string generateDefinition(bool genInline) {
            return ctx.gen.genOptional(ctx, [&](std::string &output) {
                std::string indent = genInline ? "    " : "  ";
                output += getProto(indent, genInline) + " {\n";
                if (ctx.ns == Namespace::RAII && ctx.isIndirect() &&
                    !ctx.constructor) {
                    if (ctx.cls->ownerhandle.empty()) {
                        std::cerr << "Error: can't generate funcion: class has "
                                     "no owner ("
                                  << ctx.cls->name << ", " << ctx.name << ")"
                                  << std::endl;
                    } else {
                        String name =
                            convertName(ctx.name.original, ctx.cls->superclass);

                        std::string args = createPassArguments();
                        output += "    ";
                        if (generateReturnType() != "void") {
                            output += "return ";
                        }
                        output += ctx.cls->ownerhandle + "->" + name + "(" +
                                  args + ");\n";
                    }
                } else {
                    for (const auto &p : ctx.params) {
                        if (p->getIgnoreFlag()) {
                            continue;
                        }
                        if (p->getSpecialType() ==
                            VariableData::TYPE_ARRAY_PROXY) {
                            if (p->isLenAttribIndirect()) {
                                const auto &var = p->getLengthVar();
                                std::string size = var->identifier() + "." +
                                                   p->getLenAttribRhs();
                                output += "    // if (" + p->identifier() +
                                          ".size()" + " != " + size +
                                          ") TODO\n";
                            }
                        }
                    }

                    output += generateMemberBody();
                    if (!disableCheck &&
                        ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {
                        output += generateCheck();
                    }
                     if (generateReturnType() != "void" && !returnValue.empty()) {
                        output += "    return " + returnValue + ";\n";
                    }
                }
                output += "  }\n";
            });
        }

        std::string createProtoArguments(bool declaration = false) {
            std::string args = ctx.createProtoArguments(false, declaration);
            return args;
        }

        std::string createPassArguments() {
            bool allocVar = true;
            if (allocatorVar && allocatorVar->getIgnoreFlag()) {
                allocVar = false;
            }
            std::string args = ctx.createPassArguments(true);
            return args;
        }

        std::string generateArrayCode(const MemberContext::Var &var,
                                      bool useOriginal = false) {
            if (useOriginal) {
                for (const auto &p : ctx.params) {
                    if (p->isArray() ||
                        p->original.type() != ctx.cls->name.original) {
                        p->original.set(1, p->get(1));
                    }
                }
            }

            bool convert = var->getNamespace() == Namespace::RAII;
            if (convert) {
                var->setNamespace(Namespace::VK);
            }

            std::string output;

            const auto &lenVar = var->getLengthVar();
            const std::string &id = var->identifier();
            if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {
                output += "    " + declareReturnVar();
            }

            std::string size = lenVar->identifier();
            std::string call = generatePFNcall();
            if (lenVar->original.isPointer() && !lenVar->original.isConst()) {
                output += "    " + var->declaration() + ";\n";
                output += "    " + lenVar->declaration() + ";\n";

                var->setAltPFN("nullptr");
                std::string callNullptr = generatePFNcall();

                if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {
                    output += ctx.gen.format(R"(
    do {
      {0}
      if (result == Result::eSuccess && {2}) {
        {3}.resize({2});
        {1}
        //VULKAN_HPP_ASSERT({2} <= {3}.size());
      }
    } while (result == Result::eIncomplete);
)",
                                             callNullptr, call, size, id);
                } else {
                    output += ctx.gen.format(R"(
    {0}
    {3}.resize({2});
    {1}
    //VULKAN_HPP_ASSERT({2} <= {3}.size());
)",
                                             callNullptr, call, size, id);
                }
                output += generateCheck();
                output += ctx.gen.format(R"(
    if ({0} < {1}.size()) {
      {1}.resize({0});
    }
)",
                                         size, id);
            } else {
                if (lenVar->getIgnoreFlag()) {
                    size = lenVar->getArrayVar()->identifier() + ".size()";
                } else if (var->isLenAttribIndirect()) {
                    if (lenVar->isPointer()) {
                        size += "->";
                    } else {
                        size += ".";
                    }
                    size += var->getLenAttribRhs();
                }
                output += "    " + var->declaration() + "(" + size + ");\n";
                output += "    " + call + "\n";
                output += generateCheck();
            }
            if (!returnType.empty()) {
                returnValue = generateReturnValue(id);
            } else {
                std::string parent = strFirstLower(ctx.cls->superclass);
                std::string iter = id;
                strStripSuffix(iter, "s");
                output += ctx.gen.format(R"(
    this->reserve({0});
    for (auto const &{1} : {2}) {
      this->emplace_back({3}, {1});
    }
)",
                            size, iter, id, parent);
            }
            return output;
        }

      public:
        std::string dbgtag;

        MemberResolverBase(MemberContext &refCtx)
            : ctx(refCtx), resultVar(refCtx.gen, VariableData::TYPE_INVALID) {
            returnType = strStripVk(std::string(ctx.type));
            dbgtag =
                "default"; // sc: " + std::to_string(ctx.successCodes.size());
            specifierInline = true;
            specifierExplicit = false;
            specifierConst = true;
            disableCheck = true;

            const auto &cfg = ctx.gen.getConfig();
            if (cfg.gen.dispatchParam && ctx.ns == Namespace::VK) {
                std::string type = ctx.gen.getDispatchType();
                VariableData var(ctx.gen);
                var.setFullType("", type, " const &");
                var.setIdentifier("d");
                var.setIgnorePFN(true);
                std::string assignment = cfg.macro.mDispatch.get();
                if (!assignment.empty()) {
                    var.setAssignment(" " + assignment);
                }
                ctx.params.push_back(std::make_shared<VariableData>(var));
            }
        }

        virtual ~MemberResolverBase() {}

        virtual void generate(std::string &decl, std::string &def) {
            if (ctx.generateInline) {
                decl += generateDefinition(true);
            }
            else {
                decl += generateDeclaration();
                def += generateDefinition(false);
            }
            ctx.cls->generated.push_back(ctx);
        }

        bool compareSignature(const MemberResolverBase &o) const {
            const auto removeWhitespace = [](std::string str) {
                return std::regex_replace(str, std::regex("\\s+"), "");
            };

            const auto getType = [&](const MemberContext::Var &var) {
                std::string type = removeWhitespace(var->type());
                std::string suf = removeWhitespace(var->suffix());
                return type + " " + suf;
            };

            Variables lhs = ctx.getFilteredProtoVars();
            Variables rhs = o.ctx.getFilteredProtoVars();
            if (lhs.size() == rhs.size()) {
                for (size_t i = 0; i < lhs.size(); i++) {
                    std::string l = getType(lhs[i]);
                    std::string r = getType(rhs[i]);
                    if (l != r) {
                        return false;
                    }
                }
                return true;
            }
            return false;
        }
    };

    class MemberResolver : public MemberResolverBase {        

      private:
        void transformMemberArguments() {
            auto &params = ctx.params;

            const auto transformToProxy =
                [&](const std::shared_ptr<VariableData> &var) {
                    if (var->hasLengthVar()) {

                        const auto &sizeVar = var->getLengthVar();

                        if (!sizeVar->hasArrayVar()) {
                            sizeVar->bindArrayVar(var);
                        }

                        bool isPointer = sizeVar->original.isPointer();
                        if (!var->isLenAttribIndirect()) {
                            sizeVar->setIgnoreFlag(true);
                        }

                        if (var->original.type() == "void") {
                            if (isPointer) {
                                var->setFullType("", "uint8_t", "");
                            } else {
                                var->setFullType("", "DataType", "");
                                var->setTemplate("DataType");
                                sizeVar->setIgnoreFlag(false);
                            }
                        }

                        var->convertToArrayProxy();
                    }
                };

            const auto convertName =
                [](const std::shared_ptr<VariableData> &var) {
                    const std::string &id = var->identifier();
                    if (id.size() >= 2 && id[0] == 'p' && std::isupper(id[1])) {
                        var->setIdentifier(strFirstLower(id.substr(1)));
                    }
                };

            for (const auto &p : params) {
                transformToProxy(p);
            }

            for (const auto &p : params) {
                const std::string &type = p->original.type();
                if (ctx.ns == Namespace::RAII &&
                    (ctx.nameCat == MemberNameCategory::CREATE ||
                     ctx.nameCat == MemberNameCategory::ALLOCATE ||
                     ctx.constructor)) {
                    if (ctx.gen.isHandle(type)) {
                        std::string ns =
                            ctx.gen.getConfig().macro.mNamespaceRAII.get();
                        p->toRAII();
                        convertName(p);
                    }
                }

                if (p->isArray()) {
                    convertName(p);
                    continue;
                }
                if (ctx.gen.isStructOrUnion(type)) {
                    p->convertToReference();
                    p->setConst(true);
                    convertName(p);
                }
            }
        }

        void finalizeArguments() {        
            bool hasAssignment = true;
            for (auto it = ctx.params.rbegin(); it != ctx.params.rend(); it++) {
                if (it->get()->getIgnoreFlag()) {
                    continue;
                }
                if (hasAssignment && it->get()->assignment().empty()) {
                    hasAssignment = false;
                }
                if (it->get()->type() == "AllocationCallbacks") {
                    allocatorVar = *it;
                    if (ctx.gen.getConfig().gen.allocatorParam) {
                        allocatorVar->convertToOptional();
                        if (hasAssignment) {
                            allocatorVar->setAssignment(" = nullptr");
                        }
                    } else {
                        allocatorVar->setIgnoreFlag(true);
                    }
                }
            }
        }

      protected:
        virtual std::string generateMemberBody() override {
            std::string output;
            bool immediate = ctx.pfnReturn != PFNReturnCategory::VK_RESULT &&
                             ctx.successCodes.size() <= 1;
            output += "    " + generatePFNcall(immediate) + "\n";
            if (!immediate) {
                returnValue = generateReturnValue(resultVar.identifier());
            }
            return output;
        }

      public:
        MemberResolver(MemberContext &refCtx) : MemberResolverBase(refCtx) {

            transformMemberArguments();

            if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT &&
                ctx.successCodes.size() <= 1) {
                returnType = "void";
            }
        }

        virtual ~MemberResolver() {}

        virtual void generate(std::string &decl, std::string &def) override {
            finalizeArguments();
            MemberResolverBase::generate(decl, def);
        }
    };

    class MemberResolverDbg : public MemberResolver {
      public:
        MemberResolverDbg(MemberContext &refCtx) : MemberResolver(refCtx) {
            dbgtag = "disabled";
        }

        virtual void generate(std::string &decl, std::string &def) override {
            decl += "/*\n";
            decl += generateDeclaration();
            decl += "*/\n";
        }
    };

    class MemberResolverEmpty
        : public MemberResolver { // used only for debug markup
        std::string comment;

      public:
        MemberResolverEmpty(MemberContext &refCtx, std::string comment)
            : MemberResolver(refCtx), comment(comment) {
            dbgtag = "dbg";
        }

        virtual std::string generateMemberBody() override {
            return comment.empty() ? "" : "      // " + comment + "\n";
        }
    };

    class MemberResolverPass : public MemberResolverBase {

      public:
        MemberResolverPass(MemberContext &refCtx)
            : MemberResolverBase(refCtx) {
            ctx.disableSubstitution = true;
        }

        virtual std::string generateMemberBody() override {
            return "    " + generatePFNcall(true) + "\n";
        }
    };

    class MemberResolverVectorRAII : public MemberResolver {
      protected:
        std::shared_ptr<VariableData> parent;
        std::shared_ptr<VariableData> last;
        bool ownerInParent;

      public:
        MemberResolverVectorRAII(MemberContext &refCtx, bool term = true)
            : MemberResolver(refCtx) {
            if (term) {
                last = *ctx.params.rbegin();
                last->setIgnoreFlag(true);
                last->setIgnorePFN(true);
                if (last->hasLengthVar() && !last->isLenAttribIndirect()) {
                    last->getLengthVar()->setIgnorePFN(true);
                }
                last->toRAII();
                ctx.pfnReturn = PFNReturnCategory::VOID;

                if (last->hasLengthVar() && last->isArray()) {
                    if (last.get()->original.type() == "void") {
                        if (last->isArrayIn()) {
                            last->getLengthVar()->setIgnoreFlag(false);
                        }
                    }

                    last->getLengthVar()->removeLastAsterisk();

                    last->convertToStdVector();
                } else {
                    last->convertToReturn();
                }
                ctx.useThis = true;

                returnType = last->fullType();
            }

            parent = *ctx.params.begin();
            parent->toRAII();
            ownerInParent = false;
            if (parent->original.type() != ctx.cls->superclass.original) {
                ownerInParent = true;
            } else {
                parent->setIgnorePFN(true);
            }

            std::string id = parent->identifier();

            ctx.pfnSourceOverride = id;
            if (ownerInParent) {
                ctx.pfnSourceOverride += ".get" + ctx.cls->superclass + "()";
            }

            ctx.params.rbegin()->get()->setIgnoreFlag(true);

            dbgtag = "RAII vector";
        }

        virtual std::string generateMemberBody() override {
            std::string args = createPassArguments();
            return "    return " + last->namespaceString() + last->type() +
                   "s(" + args + ");\n";
        }
    };

    class MemberResolverCtor : public MemberResolverVectorRAII {
      protected:
        String name;

      public:
        MemberResolverCtor(MemberContext &refCtx)
            : MemberResolverVectorRAII(refCtx, false), name("") {

            name = convertName(ctx.name.original, ctx.cls->superclass);
            ctx.name = std::string(ctx.cls->name);

            ctx.pfnNameOverride = name;
            if (ownerInParent) {
                ctx.pfnSourceOverride += "->";
            } else {
                ctx.pfnSourceOverride += ".";
            }

            returnType = "";

            specifierInline = false;
            specifierExplicit = true;
            specifierConst = false;

            dbgtag = "constructor";
            // disableCheck = false;
        }

        virtual std::string generateMemberBody() override {
            std::string output;

            for (const auto &p : ctx.params) {
                if (p->original.type() != ctx.cls->name.original) {
                    p->original.set(1, p->get(1));
                }
            }

            const std::string &args = ctx.createPFNArguments(true);
            const std::string &owner = ctx.cls->ownerhandle;
            std::string id = parent->identifier();
            std::string src = id;
            if (ownerInParent) {
                src += ".get" + ctx.cls->superclass + "()";
            }

            if (!owner.empty()) {
                output += "    " + owner + " = ";
                if (!ownerInParent) {
                    output += "&";
                }
                output += src;
                output += ";\n";
            }

            std::string call = generatePFNcall(false);

            output += "    " + call + "\n";
            output += generateCheck();

            if (ctx.cls->hasPFNs()) {                
                output += "    loadPFNs(";
                if (ownerInParent) {
                    output += "*";
                }
                output += src + ");\n";
            }

            return output;
        }

        bool checkMethod() const {            
            if (!ctx.gen.isHandle(parent->original.type())) {
//                 std::cout << "checkMethod: " << name.original
//                          << " not a handle (" << parent->original.type() << ")" << std::endl;
                return false;
            }
            bool blacklisted = true;
            ctx.gen.genOptional(ctx,
                                [&](std::string &output) { // REFACTOR
                                    blacklisted = false;
                                });
            if (blacklisted) {
//                    std::cout << "checkMethod: " << name.original
//                              << " blacklisted" << std::endl;
                return false;
            }
            const auto &handle =
                    ctx.gen.findHandle(parent->original.type());
            for (const auto &m : handle.members) {
                if (m.name.original == name.original) {
                    return true;
                }
            }
//            std::cout << "checkMethod: " << name.original << " not in "
//                      << handle.name << " members" << std::endl;
            return false;
        }

        std::string getName() const { return name; }
    };

    class MemberResolverUniqueCtor : public MemberResolver {

    public:
        MemberResolverUniqueCtor(MemberContext &refCtx)
            : MemberResolver(refCtx)
        {
            ctx.name = "Unique" + ctx.cls->name;

            returnType = "";

            specifierInline = false;
            specifierExplicit = true;
            specifierConst = false;

            InitializerBuilder init("        ");
            for (const auto &p : ctx.getFilteredProtoVars()) {
                if (p->type() == ctx.cls->name) {
                    init.append(ctx.cls->name, p->identifier());
                    continue;
                }
                for (const auto &v : ctx.cls->uniqueVars) {
                    if (p->type() == v.type()) {
                        std::string m = matchTypePointers(p->suffix(), v.suffix());
                        init.append(v.identifier(), m + p->identifier());
                        break;
                    }
                }
            }

            initializer = init.string();

            dbgtag = "constructor";
        }

        virtual std::string generateMemberBody() override {
            return "";
        }

    };

    class MemberResolverVectorCtor : public MemberResolverCtor {

        std::shared_ptr<VariableData> last;

      public:
        MemberResolverVectorCtor(MemberContext &refCtx)
            : MemberResolverCtor(refCtx) {

            // ctx.name = name;

            last = ctx.getLastPointerVar();

            if (last->hasLengthVar() && last->isArray()) {
                //                if (last.get()->original.type() == "void") {
                //                    if (last->isArrayIn()) {
                //                        last->getLengthVar()->setIgnoreFlag(false);
                //                    }
                //                }
                last->getLengthVar()->removeLastAsterisk();
                last->convertToStdVector();
            } else {
                std::cerr << "MemberResolverVectorCtor: can't create"
                          << std::endl;
            }

            last->setIgnoreFlag(true);
            last->setNamespace(Namespace::VK);

            specifierExplicit = false;
        }

        virtual std::string generateMemberBody() override {
            std::string output = generateArrayCode(last, true);
            return output;
        }
    };

    class MemberResolverCreateHandle : public MemberResolver {
        std::shared_ptr<VariableData> returnVar;

      public:
        MemberResolverCreateHandle(MemberContext &refCtx)
            : MemberResolver(refCtx) {
            returnVar = ctx.getLastPointerVar();
            returnVar->convertToReturn();
            returnType = returnVar->type();
            // returnType = returnVar->original.type();
            // returnVar->setType(returnType);

            // ctx.name += "Handle";
            dbgtag = "create handle";
        }

        virtual std::string generateMemberBody() override {
            const std::string &id = returnVar->identifier();
            const std::string &call = generatePFNcall();
            return ctx.gen.format(R"(
    {0} {1};
    {2}
    {3}
)",
                                  returnType, id, call,
                                  generateReturnValue(id));
        }
    };

    class MemberResolverCreate : public MemberResolver {
      protected:
        std::shared_ptr<VariableData> last;

      public:
        MemberResolverCreate(MemberContext &refCtx) : MemberResolver(refCtx) {

            last = ctx.getLastPointerVar();

            if (last->isArray()) {
                last->convertToStdVector();
            } else {
                last->convertToReturn();
            }

            last->setIgnoreFlag(true);

            last->setConst(false);
            returnType = last->fullType();

            //            if (ctx.lastHandleVariant) {
            //                last->setType(last->original.type());
            //                last->setIgnorePFN(true);
            //            }
            disableCheck = false;
            dbgtag = ctx.nameCat == MemberNameCategory::ALLOCATE ? "allocate"
                                                                 : "create";
            ctx.useThis = true;
        }

        virtual std::string generateMemberBody() override {
            std::string output;
            if (last->isArray()) {
                output += generateArrayCode(last);
            } else {
                if (ctx.ns == Namespace::RAII) {
                    last->setIgnorePFN(true);
                    const auto &first = ctx.params.begin()->get();
                    if (first->original.type() == ctx.cls->name.original) {
                        first->setIgnorePFN(true);
                    }
                    std::string args = createPassArguments();
                    output +=
                        "    return " + last->fullType() + "(" + args + ");\n";
                } else {
                    const std::string &call = generatePFNcall();
                    std::string id = last->identifier();
                    output += "    " + last->fullType() + " " + id + ";\n";
                    output += "    " + call + "\n";
                    returnValue = generateReturnValue(id);
                }
            }
            return output;
        }
    };

    class MemberResolverCreateUnique : public MemberResolverCreate {
        // std::shared_ptr<VariableData> last;
        std::string name;
        bool isSubclass = {};

      public:
        MemberResolverCreateUnique(MemberContext &refCtx)
            : MemberResolverCreate(refCtx) {

            //            last = ctx.getLastPointerVar();
            //            last->convertToReturn();
            //            last->setIgnorePFN(true);

            returnType = "Unique" + last->type();
            // ctx.pfnReturn = PFNReturnCategory::VOID;

            //            if (ctx.params.begin()->get()->original.type() ==
            //            ctx.cls->name.original) {
            //                ctx.params.begin()->get()->setIgnorePFN(true);
            //            }

            if (last->isHandle()) {
                const auto &handle = ctx.gen.findHandle(last->original.type());
                isSubclass = handle.isSubclass;
            }
            name = ctx.name;
            ctx.name += "Unique";
            dbgtag = "create unique";
        }

        virtual std::string generateMemberBody() override {
            //            const std::string &id = last->identifier();
            //            const std::string &args = createPassArguments();
            //            const auto &handle =
            //            ctx.gen.findHandle(last->original.type()); std::string
            //            call = name + "(" + args + ")"; std::string
            //            ownerArgument;
            std::string args = last->identifier();
            if (isSubclass) {
                args += ", *this";
            }
            if (ctx.gen.getConfig().gen.allocatorParam) {
                args += ", allocator";
            }
            if (ctx.gen.getConfig().gen.dispatchParam) {
                args += ", d";
            }

            std::string output = MemberResolverCreate::generateMemberBody();
            // output += "    // (" + args + ")\n";
            returnValue = generateReturnValue(returnType + "(" + args + ")");
            //            output += "    " + ctx.gen.format("{0} {1} = {2};\n",
            //            last->fullType(), id, call); output += "    " +
            //            ctx.gen.format("return {0}({1}{2});\n", returnType,
            //            id, ownerArgument);
            return output;
        }
    };

    class MemberResolverGet : public MemberResolver {
      protected:
        std::shared_ptr<VariableData> last;
        /*
                std::string generateArray() {
                    std::shared_ptr<VariableData> lenVar = last->getLengthVar();

                    std::string output;
                    std::string decl;

                    std::string arrayDecl = last->toString();
                    std::string id = last->identifier();
                    if (ctx.lastHandleVariant) {
                        last->setType(last->original.type());
                        last->setIdentifier(last->identifier() + "Handles");
                        decl += "    " + last->toString() + ";\n";
                    }
                    else {
                        decl += "    " + arrayDecl + ";\n";
                    }
                    if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {
                        decl += "    " + assignToResult("");
                    }

                    std::string call = generatePFNcall();
                    std::string finish;
                    if (ctx.lastHandleVariant) {
                        finish += "    " + arrayDecl + ";\n";
                    }
                    finish += "    " + generateReturnValue(id) + "\n";

                    if (lenVar->getIgnoreFlag()) {
                        output += "    " + lenVar->type() + " " +
        lenVar->identifier() +
                                  ";\n";
                    }

                    output += decl;
                    if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {

                        output += ctx.gen.format(R"(
            do {
              {0}
              if (result == Result::eSuccess && {1}) {
                {2}.resize({1});
                {0}
                //VULKAN_HPP_ASSERT({1} <= {2}.size());
              }
            } while (result == Result::eIncomplete);
            if (result == Result::eSuccess && {1} < {2}.size()) {
              {2}.resize({1});
            }
        )",
                                                 call, lenVar->identifier(),
        id);

                    } else {
                        if (ctx.lastHandleVariant) {
                            std::cerr << "Warning: unhandled situation in " <<
        ctx.name << std::endl;
                        }
                        last->setAltPFN("nullptr");
                        std::string firstcall = generatePFNcall();
                        output += ctx.gen.format(R"(
            {0}
            {1}.resize({2});
            {3}
            // VULKAN_HPP_ASSERT({2} <= {1}.size());
        )",
                                                 firstcall, id,
        lenVar->identifier(), call);
                    }
                    output += finish;
                    return output;
                }
                std::string generateArrayWithSize() {
                    std::string output;
                    if (ctx.lastHandleVariant) {
                        std::cerr << "Warning: unhandled situation in " <<
        ctx.name << std::endl; return "";
                    }

                    std::string id = last->identifier();
                    std::string sizeid = last->getLengthVar()->identifier();
                    output += "    " + last->toString() + ";\n";
                    if (const auto str = last->getTemplate(); !str.empty()) {
                        output += "    " + id + ".resize(" + sizeid + " /
        sizeof(" + str + "));\n";
                    }
                    else {
                        output += "    " + id + ".resize(" + sizeid + ");\n";
                    }
                    output += "    " + generatePFNcall() + "\n";
                    output += "    " + generateReturnValue(id) + "\n";

                    return output;
                }
            */

        std::string generateSingle() {
            std::string output;
            if (ctx.lastHandleVariant) {
                std::cerr << "Warning: unhandled situation in " << ctx.name
                          << std::endl;
                return "// TODO";
            } else {
                const std::string id = last->identifier();
                output += "    " + returnType + " " + id + ";\n";
                output += "    " + generatePFNcall() + "\n";
                returnValue = generateReturnValue(id);
            }
            return output;
        }

      public:
        MemberResolverGet(MemberContext &refCtx) : MemberResolver(refCtx) {
            last = ctx.getLastPointerVar();

            if (last->hasLengthVar() && last->isArray()) {
                if (last.get()->original.type() == "void") {
                    if (last->isArrayIn()) {
                        last->getLengthVar()->setIgnoreFlag(false);
                    }
                }

                last->getLengthVar()->removeLastAsterisk();

                last->convertToStdVector();
            } else {
                last->convertToReturn();
            }

            last->setIgnoreFlag(true);
            last->setConst(false);
            returnType = last->fullType();
            dbgtag = "get";
        }

        virtual std::string generateMemberBody() override {
            if (last->isArray()) {
                return generateArrayCode(last);
                //                if (last->hasLengthVar() &&
                //                last->getLengthVar()->getIgnoreFlag()) {
                //                    return generateArray();
                //                }
                //                return generateArrayWithSize();
            }
            return generateSingle();
        }
    };

    class MemberResolverEnumerate : public MemberResolverGet {
      public:
        MemberResolverEnumerate(MemberContext &refCtx)
            : MemberResolverGet(refCtx) {
            dbgtag = "enumerate";
        }
    };

    PFNReturnCategory evaluatePFNReturn(const std::string &type) const {
        if (type == "void") {
            return PFNReturnCategory::VOID;
        } else if (type == "VkResult") {
            return PFNReturnCategory::VK_RESULT;
        }
        return PFNReturnCategory::OTHER;
    }

    bool getLastTwo(MemberContext &ctx,
                    std::pair<Variables::reverse_iterator,
                              Variables::reverse_iterator> &data) { // rename ?
        Variables::reverse_iterator last = ctx.params.rbegin();
        if (last != ctx.params.rend()) {
            Variables::reverse_iterator prevlast = std::next(last);
            data.first = last;
            data.second = prevlast;
            return true;
        }
        return false;
    }

    bool isPointerToCArray(const std::string &id) {
        if (id.size() >= 2) {
            if (id[0] == 'p' && std::isupper(id[1])) {
                return true;
            }
        }
        return false;
    }

    std::string stripStartingP(const std::string &str) {
        std::string out = str;
        if (isPointerToCArray(str)) {
            out = std::tolower(str[1]);
            out += str.substr(2);
        }
        return out;
    }

    template <typename T>
    void
    createOverload(MemberContext &ctx, const std::string &name,
                   std::vector<std::unique_ptr<MemberResolver>> &secondary);

    void generateClassMember(MemberContext &ctx, GenOutputClass &out,
                             std::string &funcs);

    void generateClassMembers(HandleData &data, GenOutputClass &out,
                              std::string &funcs, Namespace ns);

    void generateClassConstructors(const HandleData &data, GenOutputClass &out,
                                   std::string &funcs);

    void generateClassConstructorsRAII(const HandleData &data,
                                       GenOutputClass &out, std::string &funcs);

    std::string generateUniqueClass(HandleData &data, std::string &funcs);

    String getHandleSuperclass(const HandleData &data);

    std::string generateClass(const std::string &name, HandleData data,
                              std::string &funcs);

    std::string generateClassRAII(const std::string &name, HandleData data,
                                  std::string &funcs);

    void parseCommands(XMLNode *node);

    void assignCommands();

    std::string generatePFNs(const HandleData &data, GenOutputClass &out);

    std::string generateLoader();

    std::string genMacro(const Macro &m);

    void initLoaderName();

    std::string beginNamespace(Namespace ns = Namespace::VK) const;

    std::string endNamespace(Namespace ns = Namespace::VK) const;

    void loadFinished();

    template<typename T>
    void configBuildList(const std::string &name, const std::map<std::string, T> &from, XMLElement *parent, const std::string &comment = "");

    struct WhitelistBase {
        std::regex rgx;
        std::string name;
        std::vector<std::string> filter;

        bool build();

        virtual bool stage();

        virtual void apply() = 0;
    };

    template<typename T>
    struct WhitelistBinding : public WhitelistBase {
        std::map<std::string, T> *dst;
        std::vector<std::pair<T*, bool>> buffer;

        WhitelistBinding(std::map<std::string, T> *dst, std::string name)
         : dst(dst)
        {
            this->name = name;
        }

        virtual bool stage() override;

        virtual void apply() noexcept override;
    };

  public:
    Generator();

    void resetConfig();

    void bindGUI(const std::function<void(void)> &onLoad);

    bool isLoaded() const { return root != nullptr; }

    void setOutputFilePath(const std::string &path);

    bool isOuputFilepathValid() const {
        return std::filesystem::is_directory(outputFilePath);
    }

    std::string getOutputFilePath() const { return outputFilePath; }

    void load(const std::string &xmlPath);

    void unload();

    void generate();

    Platforms &getPlatforms() { return platforms; };

    Extensions &getExtensions() { return extensions; };

    std::map<std::string, CommandData> &getCommands() { return commands; }

    std::map<std::string, StructData> &getStructs() { return structs; }

    std::map<std::string, EnumData> &getEnums() { return enums; }

    Config &getConfig() { return cfg; }

    const Config &getConfig() const { return cfg; }

    bool isInNamespace(const std::string &str) const;

    std::string getNamespace(Namespace ns, bool colons = true) const;

    std::function<void(void)> onLoadCallback;

    void saveConfigFile(const std::string &filename);

    void loadConfigFile(const std::string &filename);
};

#endif // GENERATOR_HPP
