parsley is based on a combination of [parsing expression grammars (PEGs)](https://en.wikipedia.org/wiki/Parsing_expression_grammar) and [Pratt parsing](https://en.wikipedia.org/wiki/Operator-precedence_parser#Pratt_parsing).

PEGs are somewhat similar to regular expressions.
The first difference between PEGs in parsley and regular expressions is the syntax. Instead of of the regular expression

```regex
[a-z]+@(mit|stanford)\.edu
```

in parsley you would write

```cpp
sequence(one_or_more(range('a', 'z')), '@', choice("mit", "stanford"), ".edu")
```

This is more verbose but also more readable. Whereas regular expressions are normally represented as a single string in C++, PEGs in parsley are represented using the native C++ character literals, strings and function calls.

The other important difference is that choices are ordered in PEG, which means that once an alternative matches, the following alternatives are no longer considered. The regular expression `a|ab` matches both `a` and `ab` but the PEG `choice('a', "ab")` will never match `ab` since the first alternative already matches the `a`. In order to match both `a` and `ab` you would have to rewrite your PEG to `sequence('a', optional('b'))`.
