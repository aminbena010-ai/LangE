# Ejemplo: FizzBuzz

```rivo
fn main():
    var n:int = 1
    while n <= 15:
        if n % 15 == 0:
            print("FizzBuzz")
        elif n % 3 == 0:
            print("Fizz")
        elif n % 5 == 0:
            print("Buzz")
        else:
            print(n)
        n += 1
```

La cadena `if`/`elif`/`else` evalúa las condiciones en orden y ejecuta solo
la primera que sea verdadera; si ninguna lo es, corre el `else`.
