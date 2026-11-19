# Firmware `cism216.fw`

Este repositorio **no incluye** `cism216.fw`. Es firmware propietario de
Plustek y no se redistribuye sin permiso.

## Cómo obtenerlo

Extrae `cism216.fw` de un instalador oficial de Plustek OpticSlim M12 o de
IRIScan Express 2 que corresponda a tu escáner. No uses firmware de otro
modelo ni de sitios no oficiales.

- Nombre esperado: `cism216.fw`
- Tamaño: **8192 bytes**
- SHA-256 de la copia usada en la prueba:
  `9730A258DC7152184A9C4FB7C8D12ABB3B4DE537276F4686525BEB6A72F98C15`

Verifica el tamaño antes de usarlo:

```bat
for %F in (cism216.fw) do @echo %~zF
```

Debe imprimir `8192`.

## Dónde colocarlo

Puedes dejarlo en una de estas rutas antes de ejecutar `install.cmd`:

```
cism216.fw
firmware\cism216.fw
..\pkg\cab_extract\cism216.fw   (estructura usada por el autor)
```

`install.cmd` lo copia a `%SystemRoot%\twain_32\Color USB Scanner\`.

## Aviso

Si el firmware no es el correcto, la subida puede fallar y el escáner puede
quedar en un estado inconsistente. No desconectes el USB durante la subida.
