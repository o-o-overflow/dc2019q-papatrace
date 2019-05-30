papatrace
=========

**ABSTRACT:** This service provides a limited interface to the [angr binary analysis framework](http://angr.io/), and a target program that will print a limited number of bytes of the flag. The user is able to use angr through this service to analyze the target program. The intended vulnerability of this service is to exploit a hash-collision vulnerability within angr, which allows the user to forcefully set a constrained value, resulting in an altered program control flow during symbolic execution and a full print of the flag file.

---

### Part 1: Getting Started

We start to solve the challenge by downloading the service public files, which include pitas.py and flagleak. pitas.py is the source code to the service itself, and should be reviewed to see how it interacts with angr ([source code](https://github.com/o-o-overflow/dc2019q-papatrace/blob/master/service/pitas.py)). flagleak should be reversed to understand its behavior ([source code](https://github.com/o-o-overflow/dc2019q-papatrace/blob/master/service/bins/flagleak.c)).

In summary, flagleak does the following:

-   Reads the flag file into a buffer on the stack.
-   Reads an 8-byte integer from stdin into a variable `i`.
-   Check to see if the least-significant byte of `i` is greater than 9, in this case the program exits.
-   The program enters a `for` loop, printing `i % 256` bytes of the flag. Under normal conditions, this could only possibly be 0-9.

In summary, the pitas.py service gives the user the following abilities sufficient to solve this challenge:

-   Loads the target program (flagleak) for analysis.
-   Provides an interface to create concrete/symbolic values as part of the stdin stream.
-   Provides an interface to step the symbolic execution, concretize register values, and print the contents of the stdout stream.

The goal is to get the target program to leak the full contents of the flag. To do this, we need `i` to be at least as large as the length of the flag string (&gt;9), but small enough to survive the initial length check (&lt;=9). This contradiction would not normally be possible to satisfy. However, we can exploit a weakness in angr to subvert this limitation.

### Part 2: Exploit Idea

We want to increase `i` to be a large enough number to print the full flag, but `i` must be small enough to survive the first check. If we are able to make it through the first check, we can interact with angr to change what angr "thinks" the value of `i` should be, before the flag print loop begins. To do this, we will exploit a particular optimization in a module of angr called [claripy](https://github.com/angr/claripy/).

angr uses claripy internally to manage and solve constraints on the program state. These constraints can include symbolic and concrete values for registers/memory, in the form of bitvectors represented as abstract syntax trees (AST) of operations. See [here](https://docs.angr.io/core-concepts/solver) for more information about angr’s constraint solver.

AST objects have a property of "hash identity," which means that two structurally equivalent AST objects will hash to the same value. That is, calling the Python built-in function hash will return the same value for two structurally identical AST objects. Additionally, to improve memory efficiency, previously created AST’s will be cached in a Python dictionary internal to the class itself, and the dictionary is indexed by the 64-bit hash value returned by the aforementioned hashing function. When an AST is created, [a lookup is performed](https://github.com/angr/claripy/blob/112e62fcf6b36332737b326ffe209764c4876ea4/claripy/ast/base.py%23L174). If the AST has already been created it will be returned, otherwise it will be added to the cache. This hashing function used by claripy is relatively fast, but susceptible to collisions; this, combined with the concept of the AST cache, form the basis of this exploit.

To exploit angr, we will target this AST [hashing function](https://github.com/angr/claripy/blob/112e62fcf6b36332737b326ffe209764c4876ea4/claripy/ast/base.py%23L196) used by claripy. We will cause angr to place an object in the AST cache ahead of time which, when hashed, will collide with a value we know it will try to create in the future. At the opportune moment of our choosing (just after the first check), we will force angr to try to create an AST to represent the value of the register containing `i` (the number of flag bytes to print), and it is at this moment that our previously created AST will be returned instead, yielding the value we want.

### Part 3: Finding Collisions

If we inspect the [hashing function](https://github.com/angr/claripy/blob/112e62fcf6b36332737b326ffe209764c4876ea4/claripy/ast/base.py%23L196), we can determine exactly how the AST will be hashed: it is the first 8 bytes of the MD5 hash of a "pickled" tuple value. By running the pitas.py service locally, we can augment the service to allow us to enter an iPython shell. We can then determine exactly what the tuple looks like that will be hashed; the tuple will be of the form:

    ('BVV', (0x5858585858585858, 0x40), '64', 0x7918a246f6f8, False, 0x35d373)

The second element of the outer tuple contains an inner tuple of (value, bits). In this context, this will be the value that we will attempt to set `i` to.

We now need to find two values which, when placed in this tuple and hashed, result in the same hash. Additionally, we have the following constraints on our possible candidate values: the least significant byte of one of the values must be &lt;= 9, and the least significant byte of the other value must be at least as large as the flag length.

We can construct a [program](https://github.com/o-o-overflow/dc2019q-papatrace/blob/master/brute.c) to enumerate all collisions by sweeping through the domain of possible values, calculating the corresponding hash, then finding the colliding hashes. Two such values which result in the same hash and meet the necessary requirements are:

- Value \#1: 0x2c0ae81ee3f30c08
- Value \#2: 0x2c45e81ee42e52c3

We can confirm that these values result in a hash collision using the following snippet of Python code:

    In [36]: def get_hash(value):
            ...:         obj = ('BVV', (value, 0x40), '64', 0x7918a246f6f8, False, 0x35d373)
            ...:         return struct.unpack('<QQ', hashlib.md5(pickle.dumps(obj,-1)).digest())[0]
            ...: print(hex(get_hash(0x2c45e81ee42e52c3)))
            ...: print(hex(get_hash(0x2c0ae81ee3f30c08)))
    0xf60c0ed0cdba8f
    0xf60c0ed0cdba8f

### Part 4: Exploiting the Service

Now that we have the two "magic" values to pass to the program, we can exploit the service as follows. Remember, if we are able to make it through the first check in the program, we can change what angr "thinks" the value of `i` should be, before the loop begins, to a larger number, resulting in a full print of the flag through exploitation of the AST hashing/cache.

When starting a trace, the client is able to add add constrained symbolic variables and add concrete values to the standard input stream. We will add the magic values to the stdin stream in three parts: the first byte of the first value (`08`) is passed as a constrained symbolic value, the next 7 bytes of the first value (`0cf3e31ee80a2c`) are passed as a concrete value, and the full 8 bytes of the second value in big-endian encoding (`2c45e81ee42e52c3`) are also passed as a concrete value. There are a couple of important details to be aware of about this:

1.  About concrete vs symbolic inputs: We must split the first value into symbolic and concrete components because later we will force angr to concretize the value of a register, and we will want to collide against the second value, which will already be in the AST cache at that point.
2.  About endianness: The first value is passed in little endian as it is read into a register during symbolic execution, and angr correctly models this read based on what it knows about the CPU architecture (x86-64). The second value is passed as big-endian. Big-endian is the default endianness used by angr, so to get angr to store the second value correctly, we must use big-endian at this step.

We can now begin symbolic execution of the program, stepping until just after the program passes the first check during symbolic execution (13 steps). We will pass the first check because the first byte of the first value was not greater than 9. We can then concretize the `rbx` register, which contains `i`. By doing this, we force angr to resolve `rbx` and it will consume the first byte and 7 following bytes forming a concrete bitvector value. When angr creates this bitvector, it will result in a lookup in the AST cache. At this point, the hash collision occurs and the second value which we passed in will be returned. This results in `rbx` being set to 0xC3. Now we can continue symbolic execution and enough iterations of the print loop will occur to get a full print of the flag.

[The full exploit script can be found here.](https://github.com/o-o-overflow/dc2019q-papatrace/blob/master/interaction/exploit1.py)
