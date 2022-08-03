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

assert 0 "0;"
assert 42 "42;"
assert 114 "81+42-9;"
assert 41 "12 + 34 - 5;"
assert 7 "1+2*3;"
assert 9 "(1+2)*3;"
assert 70 "1 + 2 * 3 - 4 / 2 + 5 * (6 + 7);"
assert 4 "-1+2--3;"
assert 1 "- -1;"
assert 1 "- - +1;"
assert 2 "-1+++++2-----3++++++4;"

assert 0 "1==0;"
assert 1 "42==42;"
assert 1 "1!=0;"
assert 0 "42!=42;"

assert 1 '0<1;'
assert 0 '1<1;'
assert 0 '2<1;'
assert 1 '0<=1;'
assert 1 '1<=1;'
assert 0 '2<=1;'

assert 1 '1>0;'
assert 0 '1>1;'
assert 0 '1>2;'
assert 1 '1>=0;'
assert 1 '1>=1;'
assert 0 '1>=2;'

assert 3 "1; 2; 3;"
assert 11 "1+2; 3+4; 5+6;"
assert 1 "1<2; 2>1;"
assert 0 "1>2; 2<1;"

assert 3 'a=3; a;'
assert 8 'a=3; z=5; a+z;'
assert 6 'a=b=3; a+b;'

echo OK
