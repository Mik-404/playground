# Formal Languages 2025

| Student | Isaev Ivan |
|---------|-----|
| Group   | 415 |

---
Реализовано преобразование eps-NFA -> NFA -> DFA -> min-CDFA -> regepx. Можно стартовать и завершаться на любом этапе.
For usage information, run the program with the `-h` flag.

Регулярка должна быть в формате:

- Допустимы символы латинского алфавита в качестве символов
- `|` объединение языков
- `+` плюс клини
- `*` звезда клини
- `.` конкатенация, но её можно пропускать
- `ϵ` эпсилон

Для представления автоматов используется следующий подход:
```
    {
        "S" : [
            "s0",
            "s1",
            "s2"
        ],
        "start" : "s0",
        "F" : [
            "s1",
            "s2"
        ],
        "delta" : [
            {
                "from" : "s0",
                "to" : "s1",
                "sym": "a"
            }
        ]
    }
```

# To build and run:
`cmake -B build`

`cd build`

`cmake --build .`

# Example run:
`./my_program --input-file=inp.txt --input-type=regex -o 1.txt --last-stage=4 -d`

# Help
Allowed options:

- -h [ --help ]                         Produce help message

- -d [ --debug ]                        Enable debug info
- -o [ --output-file ] arg (=out_graph.dot)
                                        Output file path
- -i [ --input-file ] arg               Input file path. istream will be read
                                        by default.
- -f [ --first-stage ] arg (=-1)        Number of the first processing stage of
                                        the FA or Regex; the numbers correspond
                                        to those given in the assignment.
- -l [ --last-stage ] arg (=-1)         Number of the last processing stage of
                                        the FA or Regex; the numbers correspond
                                        to those given in the assignment.
- --input-type arg                      FA or Regex will be submitted as input.
                                        Possible inputs are `fa` and `regex`

# stonks 🚀