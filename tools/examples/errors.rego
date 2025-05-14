package errors

a := 5 if {
    b > 2
}

# recursion error
b := 3 if {
    a == 5
}

# double assignment
c := x if {
	x := 2
    x := 3
}