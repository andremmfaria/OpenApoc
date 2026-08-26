OpenApoc Coding Style Guidelines
================================

This document specifies the guidelines for writing and formatting the C++ code that forms the core of OpenApoc. For the wider contribution workflow, build/debug commands, issue tracking, and clean-room rules for original-game compatibility work, see `DEVELOPMENT.md`.

Globally, we use standard C++17. This requires reasonably modern compilers (GCC 8, MSVC 2019+, and Clang 7+ have been tested). You should avoid compiler-specific extensions where possible. Exceptions to this exist, but should be wrapped in a preprocessor check:
```C++
#ifdef _MSC_VER
MSVCIsCrazySometimes
#else
// Everything else we support (gcc + clang) are pretty much extension-compatible
GCCIsntMuchBetter
#endif
```

C++17 features are encouraged. Patterns from older C++ versions that have been superseded should be avoided where the newer form is clearer.

The formatting sections of this document are enforced by the [clang-format tool](https://releases.llvm.org/18.1.1/tools/clang/docs/ClangFormat.html). The CI workflow currently uses `clang-format-18`; use the same major version locally where possible. The configuration file `.clang-format` in the repository root is the source of truth for formatting.

Run `clang-format` on modified C++ files before check-in:

```
clang-format path/to/file.cpp path/to/file.h
```

When run from the repository root, it should automatically use the supplied `.clang-format` configuration file. The tool also supports modifying the supplied source files to match the configured format when passed the `-i` flag:

```
clang-format -i path/to/file.cpp path/to/file.h
```

When using CMake, there is a `format-sources` target that will run `clang-format -i` on configured source files within the OpenApoc repository tree:

```
cmake --build build --target format-sources
```

The broader `format` target also formats XML, form, font, and alias files when `xmllint` is available:

```
cmake --build build --target format
```

`clang-tidy` is configured via `.clang-tidy` in the repository root and currently enables clang diagnostics, clang static analyzer checks, and `readability-non-const-parameter`. CI uses `clang-tidy-18`. It can be run locally with the `tidy` target after configuring a CMake build directory:

```
cmake --build build --target tidy
```

Indent:
-------
* Tabs for indenting, spaces for alignment, indenting by 1 tab for each new scope
```C++
void function()
{
	reallyLongFunctionNameWithLotsOfArguments(argOne, argTwo,
	                                          argThree);
}
```
* Avoid going over 100 columns (at tab width of 4 spaces).
  * If you find yourself going over this it's often a hint to try to pull things out of loops/into functions
  * Don't break strings up to fit this, it looks ugly and makes things even harder to read.
* If you have to break, indent the following line by an extra tab
  * Let `clang-format` handle operator placement. The repository configuration normally keeps binary operators at the end of the previous line.
```C++
void reallyLongFunctionNameIMeanThisIsReallyBadlyNamedWhateverIDontCareTheyPayMeAnyway(int parameterOne,
	int paramTwo, char theThirdOne)
{
	if (parameterOne == yetAnotherReallyLongLineHereComeSomeWordsBlaBlaBlaAreYouStillReadingThisComeOnDont
		&& youHaveBetterThingsToDo)
	{
		doWhatever();
	}
}
```

Whitespace:
-----------
* Spaces before and after operators
```C++
	a = b;
	a && b;
	a + b;
```
* Space after flow control keywords such as if/else/for/while/switch, and spaces around `:`/`;` in `for`
```C++
	for (auto &a : b)
```
* No spaces after function name (or function-like keywords like 'sizeof'), but space after flow control keywords, space after comma for multiple args
```C++
	func(a, b);
	if (a == 0)
```
* References and pointers: & and * align to right (to variable) not type
```C++
	float *pointerToFloat;
```

Scope:
------
* Indent 1 tab for each new scope
* New scope is _always_ surrounded by {} braces
* New scope has a `{` on the next line at the indent of the old scope, not the new scope
* closing scope `}` same indent as opening `{`, again on a new line
* New scope caused by:
  * Functions
```C++
void functionDefinition()
{
	newScopeHere();
}
```
  * new conditional block (if/else/when/for)
```C++
	if (x)
	{
		doWhatever();
	}
	else if (y)
	{
		doWhateverTheSecond();
	}
	else
	{
		doThatLastThing();
	}

```
  * 'switch'
  * 'case' also indents a new scope. {} are optional, based on if new stack variables are needed to handle the case.
    * Note switch should always have a default case unless over an enum class (where they may not if (and only if) every value is handled)
    * All 'case' sections should have a 'break'
```C++
	switch (a)
	{
		case A:
			doSomething();
			break;
		case B:
		{
			auto var = somethingElse();
			doSomethingElse(var);
			break;
		}
		default:
			doDefaultCase();
			break;
	}
```
  * Class/enum/struct declarations
    * note: public/private/protected are an exception to this, being aligned to 'class' keyword, not 'within' it's scope
```C++
class MyClass
{
private:
	int privateVariable;
public:
	void publicFunction();
};
```
* Exception to this is 'trivial' functions that have the definition and contents all on one line
  * 'Trivial' is defined by a single statement that fits within the 100-column limit
```C++
int Class::function() { return 0; }
```
* New scope is not caused by:
  * namespace (which should also have a comment stating the namespace name at the closing bracket)
```C++
namespace OpenApoc
{
class MyClass
{
private:
	int x;
};
} // namespace OpenApoc
```
  * labels
* Labels and #preprocessor directives /always/ on column 0 (start of line) no matter the scope
```C++
#if defined(LINUX)
	x = linuxFunction();
#else
	x = otherFunction();
#endif
	if (x)
	{
		goto error;
	}
error:
	return 0;
```

Case:
-----
* Namespaces should be CamelCase
```C++
namespace OpenApoc
```
* Classes and enums should be CamelCase
```C++
class MyClass
```
* 'enum class' members should be CamelCase
```C++
enum class MyEnum
{
	ValueOne,
	ValueTwo,
};
```
* class methods and member variables (public/private/protected) should be camelBack
```C++
class MyClass
{
public:
	void someFunction();
	int someVariable;
};
```
* Function parameters (public/private/protected) should be camelBack
```C++
int function(int parameterOne, char secondOne)
```
* Variables should be camelBack
  * Don't be afraid to use 'short' variable names if it's obvious
```C++
void function()
{
	int localVariable = 0;
	int x = 1;
	int y = 5;
	for (int i = 0; i < 5; i++)
```
* class/global constants should be SHOUTY_CAPS, along with _all_ macros
```C++
#define OPENAPOC_VERSION "1.0"
```
* Labels should be lower_case:
```C++
exit_loop:
	goto exit_loop;
```
* All members should be initialised in all constructors if they don't have a default constructor. You can use member initialisation in the class definition if this is clearer
```C++
class MyClass
{
	int variableMember = 0;
};
```

Types:
------
* Avoid typedef - use the `struct` keyword where necessary in C-like code
```C++
struct MyStruct
{
	int x;
};

void myStructUser(struct MyStruct s)
```
* `up<>`, `sp<>`, and `wp<>` aliases are defined for `std::unique_ptr<>`, `std::shared_ptr<>`, and `std::weak_ptr<>` in `library/sp.h`. Use them instead of the verbose versions.
* `mksp<T>(args...)` and `mkup<T>(args...)` helper functions are also provided in `library/sp.h` as aliases for `std::make_shared<T>()` and `std::make_unique<T>()` respectively. Use these to construct smart pointers.
* Use anonymous namespaces for 'file-local' stuff (instead of static, as you can wrap classes in it too)
```C++
namespace
{
void localFunction()
}; // anonymous namespace
```
* We provide a `UString` class. This should be used for _all_ strings, as it provides platform-local non-ASCII charset handling
  * All `char *`/`std::string` params are assumed to be in UTF-8 format.

Templates:
----------
* If templates help, go ahead, don't avoid them
* prefer 'typename' to 'class'
* template types should be CamelCase
```C++
template <typename LocalType> function(LocalType param)
```
* 'short' typenames are OK if it's obvious what's going on
```C++
template <typename T> Class<T>::function()
```

Class declarations:
-------------------
* member functions camelCase()
* 'public:' 'private:' 'protected:' are indented to the 'class' keyword, everything within them indented to class scope.
  * Always use 'private', even if that's the default
```C++
class MyClass
{
private:
	int localVariable;
public:
	void publicFunction();
};
```
* 'virtual' keyword only used for base class, 'override' used for derived
  * All classes with a virtual (or overridden) function _must_ specify a virtual destructor
* Inheritance should be on the same line as the 'class' keyword (until you get to the column limit and break)
```C++
class BaseClass
{
public:
	virtual ~BaseClass();
	virtual void someFunction();
};

class SubClass : public BaseClass
{
public:
	void someFunction() override;
};
```
  * Never use both 'virtual' and 'override'
* Don't define an empty {} body in the header for constructors/destructors etc. - use '= default' instead
```C++
class MyBaseClass
{
public:
	virtual ~MyBaseClass() = default;
};
```
* define pure virtual "virtual void func() = 0;" for 'interface' classes
```C++
class MyInterface
{
public:
	virtual void functionBaseClassesMustOverride() = 0;
};
```
  * No need for 'pure' interface classes, they can have code that all subclasses will use!
* For trivial initial values prefer initialisers in the class declaration (It's easier to see what's set and cleans up constructor definitions)
```C++
class MyClass
{
public:
	Type initialisedMember = 0;
};
```
* In constructors prefer initialisation of members with an initialiser list over assignment
  * Good:
```C++
MyClass::MyClass(Type value) : member(value)
{
	doWhatever();
}
```
  * Bad:
```C++
MyClass::MyClass(Type value)
{
	member = value;
	doWhatever();
}
```
* Initialisers should be in order of declaration in the class
  * For example with the class:
```C++
class MyClass
{
public:
	Type memberA;
	Type memberB;

	MyClass(Type valueA, Type valueB);
};
```
  * Good:
```C++
MyClass::MyClass(Type valueA, Type valueB) : memberA(valueA), memberB(valueB) {}
```
  * Bad:
```C++
MyClass::MyClass(Type valueA, Type valueB) : memberB(valueB), memberA(valueA) {}
```
* Use 'struct' for 'data-only' types
  * Structs should _never_ have public/private/protected declarations, if there's anything non-public you shouldn't use a struct.
  * Likely only going to be used within data reading/writing to files
  * Because of this you're probably going to need to use fixed-width types (see <cstdint> header)
```C++
struct DataFileSection
{
	uint32_t x;
	uint32_t y;
	uint32_t z;
};
```
  * If using a struct to read in data, use a static_assert to ensure correct sizing:
```C++
static_assert(sizeof(DataFileSection) == 12, "DataFileSection not expected 12 bytes");
```
* If ownership of a member is tied to the class, don't use a pointer and new/delete in constructor/destructor. Just use the type and initialise it correctly in the init list before the constructor.
  * If the above is not possible (e.g. complex init requirements, 'may be invalid and null' use a up<>
* If we /know/ a member reference owned by another object will be live for the duration of the class, use a &reference member
* Otherwise use a sp<>/up<> depending if it should take a reference and if having a 'null' object makes sense.

Functions:
----------
* Const functions where possible (IE not modifying any members)
```C++
class MyClass
{
public:
	int dataAccessor() const;
};
```
* Const params where logically not to be modified
```C++
void function(const Type& readOnlyParam)
```
* Const returns where the caller should never modify
```C++
const Type& functionWhereYouCantModifyMyReturnThanks()
```

General code:
-------------
* Where possible use auto when it keeps the type obvious from the right-hand side
```C++
	auto variableName = function();
```
  * Note where auto& may be better to avoid a copy
* sp<> up<> wp<> smart pointers
  * Use mksp<>() and mkup<>() to construct them instead of new
```C++
	auto var = mksp<Type>(args);
	auto uniqueVar = mkup<Type>(args);
```
  * Use std::move to transfer up<> ownership
```C++
	auto var = mkup<Type>(args);
	functionThatTakesOwnershipOfParam(std::move(var));
```
* Never use a 'naked' new - they should always be packaged immediately in a smart pointer
* Use `emplace()` in STL containers when constructing an object in place is clearer or avoids an unnecessary copy
  * Use `insert()` when inserting an existing object is clearer
* Use foreach loops where possible ( "for (auto &element : container)")
```C++
	for (auto &element : container)
	{
		whatever(element);
	}
```
  * Exception may be a 'safe' iterator when possibly removing elements during loop, then use iterator and keep copy locally
* Where possible use 'enum class'
* Naming variables - don't be afraid of using short names (`i`) if their use is obvious
* While 'descriptive' names are nice, shorter code is also nice. Don't repeat context
  * 'x' is fine is we already know we're doing something in coordinate space, no need to name it theXCoordinateOfTheCityMapInTiles
* Reading code is important - try to make it flow
  * avoid 'yoda conditionals' (1 == var) don't help, modern compilers catch a =/== typo easily
  * if post increment (x++) flows better use that, don't try to 'optimise' away the copy - the compiler will do that for you
* The compiler is more clever than you could ever possibly hope to be. Write things to be clear and obvious. Only /after/ it's proven to be a problem to you even look at optimisation (then _always have numbers_)
* Don't use C casts (`(int)x`) - that does different things depending on whether the object type has a defined conversion or not. Use `static_cast<>`/`reinterpret_cast<>` instead.
* prefer {} constructor calls where possible
```C++
	MyClass classInstance{argumentOne, argTwo};
```
  * Requires you to avoid implicit conversions - this is good!
* STL initialiser lists are good
```C++
	std::vector<int> someInts = {1, 2, 3};
```
* static_assert() any assumptions WRT alignment/packing (when reading structs from files, for example) - or any template restrictions (e.g. if this is only valid on unsigned types, check it!)
* <limits> is preferred to 'c' MAX_INT/whatever
```C++
	auto maxInt = std::numeric_limits<int>::max();
```
* RAII where possible
* Early-return is cool, go ahead
```C++
	if (dontHaveToDoAnything)
	{
		return;
	}
	doLotsOfBoringStuff();
```
* goto: is useful in some specific cases (e.g. breaking out of nested loops) - but only use it where another keyword won't do what you want
  * Note limitations WRT goto: over stack initialisers - IE you can't do it :)
```C++

	for (auto &x : containerX)
	{
		for (auto &y : x.containerY)
		{
			if (weShouldStopAt(y))
			{
				goto end;
			}
		}
	}
end:
	return;
```

Logging:
--------
* LogInfo/LogWarning/LogError take fmt-style format strings (using the ``fmt`` library). Use ``{0}``, ``{1}``, etc. as positional placeholders, or ``{}`` for sequential arguments.
```C++
	LogInfo("Starting OpenApoc \"{0}\"", OPENAPOC_VERSION);
	LogWarning("Value {0} exceeds limit {1}", value, limit);
	LogError("Failed to load file \"{0}\"", path);
```
* `LogInfo` is cheap, but keep it useful. Prefer logging meaningful state changes, diagnostics, and rare events over noisy per-frame/per-tick chatter.
* LogWarning should be something that has gone wrong, but recoverable.
* LogError is for fatal errors.

Comments:
---------
* either // or /* */ is fine - prefer // for single line
* If doing multi line /\*-style comments have an aligned '\*' at the beginning of each subsequent line:
```C++
/* first line
 * second line
 * last line */
```
* Don't comment for the sake of it
  * Try to make the code clearer first if a comment is 'required' to make something obvious
  * Function/variable names are useful here - if reading it aloud describes what your comment was going to say that's perfect
  * //TODO: //FIXME: when leaving known breakage

Clean-room notes:
-----------------
If a change is based on original-game research, follow the clean-room and copyright rules in `DEVELOPMENT.md#clean-room-and-copyright-rules`. Do not copy original source, decompiler output, disassembly listings, proprietary assets, or generated dumps of original resources into OpenApoc code or documentation.

Includes:
---------
* "local.h" files first
* then <system.h> includes
* Within each of the 2 blocks try to keep them alphabetically sorted (some exceptions may happen, if there's a system dependency not managed by the system header itself)
* local headers always relative to the root of OpenApoc - even if in the same directory
  * e.g. "framework/event.h" not "../event.h" or "event.h"

Headers:
--------
* prefer "#pragma once" to "#ifndef __HEADER_NAME" include guards
* Headers should avoid #include "dependency.h" where possible
  * do forward declaration of classes instead where possible
```C++
class ForwardDeclaredType;

void someFunction(ForwardDeclaredType &param);
```
  * 'not possible' includes templates, non-class types, superclasses, try building it without and see what fails
