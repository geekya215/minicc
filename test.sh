#!/bin/bash
assert() {
  expected="$1"
  input="$2"

  ./minicc "$input" > tmp.s || exit

  riscv64-linux-gnu-gcc -static -o tmp tmp.s
  qemu-riscv64-static ./tmp

  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert 0 '{ return 0; }'
assert 42 '{ return 42; }'
assert 114 '{ return 81+42-9; }'
assert 41 '{ return 12 + 34 - 5; }'
assert 7 '{ return 1+2*3; }'
assert 9 '{ return (1+2)*3; }'
assert 70 '{ return 1 + 2 * 3 - 4 / 2 + 5 * (6 + 7); }'
assert 4 '{ return -1+2--3; }'
assert 1 '{ return - -1; }'
assert 1 '{ return - - +1; }'
assert 2 '{ return -1+++++2-----3++++++4; }'

assert 0 '{ return 1==0; }'
assert 1 '{ return 42==42; }'
assert 1 '{ return 1!=0; }'
assert 0 '{ return 42!=42; }'

assert 1 '{ return 0<1; }'
assert 0 '{ return 1<1; }'
assert 0 '{ return 2<1; }'
assert 1 '{ return 0<=1; }'
assert 1 '{ return 1<=1; }'
assert 0 '{ return 2<=1; }'

assert 1 '{ return 1>0; }'
assert 0 '{ return 1>1; }'
assert 0 '{ return 1>2; }'
assert 1 '{ return 1>=0; }'
assert 1 '{ return 1>=1; }'
assert 0 '{ return 1>=2; }'

assert 3 '{ 1; 2; return 3; }'
assert 11 '{ 1+2; 3+4; return 5+6; }'
assert 1 '{ 1<2; return 2>1; }'
assert 0 '{ 1>2; return 2<1; }'

assert 3 '{ a=3; return a; }'
assert 8 '{ a=3; z=5; return a+z; }'
assert 6 '{ a=b=3; return a+b; }'
assert 3 '{ foo=3; return foo; }'
assert 8 '{ foo123=3; bar=5; return foo123+bar; }'
assert 10 '{ foo123=3; bar=5; fizz=2; return foo123+bar+fizz; }'

assert 1 '{ return 1; 2; 3; }'
assert 2 '{ 1; return 2; 3; }'
assert 3 '{ {1; {2;} return 3;} }'

echo OK
