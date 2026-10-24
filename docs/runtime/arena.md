# Arena de memoria

El runtime reserva almacenamiento para valores dinámicos y conserva
referencias a lo largo de la ejecución del programa. Este modelo simplifica el
ciclo de vida desde Rivo: no hay operador `free` ni destructores de usuario.

```rivo
fn main():
    var texto:string = "arena"
    var lista:list<string> = [texto]
    print(lista[0])
```

Consulta `src/runtime/` para los límites reales de la implementación; el
detalle del asignador no constituye una promesa de ABI.
