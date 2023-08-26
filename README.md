# cpp-minithesis

This is a port of [Minithesis](https://github.com/drmaciver/minithesis) in C++, with the intent to try using it in SerenityOS.

## Why?

What do you mean?

### Why property-based testing?

It's great! Tests edge cases you didn't/couldn't think of; increases your confidence that the program works the way you think it does.

### Why Minithesis instead of QuickCheck?

It uses an "internal shrinking" approach, which removes the burden of writing shrinkers from the user, and works well in face of monadic bind. This (IMHO) makes it superior to QuickCheck approach (manual/codegen'd shrinkers) and to the "integrated shrinking" lazy rose tree approach (ie. Hedgehog).
