package multi

default a := 0

a := val if {
    input.a > 0
    input.a < 10
    input.a % 2 == 1
    val := input.a * 10
} {
    input.a > 0
    input.a < 10
    input.a % 2 == 0
    val := input.a * 10 + 1
}

a := input.a / 10 if {
    input.a >= 10
}