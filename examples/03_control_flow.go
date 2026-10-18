package main

func main() int {
    x := 10
    if x > 5 {
        println(1)
    } else {
        println(0)
    }

    sum := 0
    i := 0
    for i < 10 {
        sum = sum + i
        i = i + 1
    }
    println(sum)
    return sum
}
