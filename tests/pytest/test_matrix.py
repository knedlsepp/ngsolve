import pytest
from ngsolve import *

def test_matrix():
    n = 10
    m = 15
    a = Matrix(n,m)
    assert a.Height() == n
    assert a.h == n
    assert a.Width() == m
    assert a.w == m

    a[:] = 99
    a[5,:] = 55
    a[2,4] = 1
    assert a[1,2] == 99
    assert a[2,4] == 1
    assert a[5,4] == 55
    a[:,3] = 33
    assert a[2,3] == 33

def test_matrix_numpy():
    try:
        import numpy
    except:
        pytest.skip("could not import numpy")
    n = 10
    m = 15
    a = Matrix(n,m)
    assert a.Height() == n
    assert a.h == n
    assert a.Width() == m
    assert a.w == m

    a[:] = 99
    a[2,4] = 1
    assert a[1,2] == 99
    assert a[2,4] == 1

    b = a.NumPy()
    assert b[1,2] == 99
    b[1,2] = 40
    assert a[1,2] == 40
    a[1,2] = 20
    assert b[1,2] == 20

    c = Matrix(n, m, True)
    c[0,1] = 1j
    d = c.NumPy()
    assert d[0,1] == c[0,1]
    d[0,1] = 1+3j
    assert d[0,1] == c[0,1]
