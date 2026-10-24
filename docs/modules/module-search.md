# Búsqueda de módulos

Para `import util`, Rivo busca `util.ro` en el directorio del archivo principal,
los directorios añadidos con `-I`, la ruta configurada `RIVO_LIB` si existe y
la biblioteca estándar asociada al compilador.

```sh
./build/rivo app.ro -I ./modulos -I ./compartidos -o app
```

Las rutas `-I` se pueden repetir. Coloca módulos locales junto al programa o
añade explícitamente su carpeta; no dependas solo del directorio actual del
shell.
