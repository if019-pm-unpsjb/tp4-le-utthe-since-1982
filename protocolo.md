# Introducción al Protocolo de Chat y Transferencia de Archivos

Este documento describe el protocolo de comunicación utilizado para un sistema de chat que permite también la transferencia de archivos entre los clientes conectados a un servidor central. El protocolo cubre las operaciones básicas de mensajería, la gestión de la conexión de los clientes y el envío/recepción de archivos. Esta documentación permite desarrollar un servidor y/o un cliente compatible con el sistema sin necesidad de estudiar el código de implementación detallado

## Conexión Inicial

### Cliente

Establece una conexión TCP con el servidor usando la dirección IP y el puerto proporcionados.
Envía el nombre del usuario como primer mensaje tras establecer la conexión. Este nombre debe tener entre 2 y 31 caracteres.

### Servidor

Acepta conexiones entrantes y crea un nuevo hilo para cada cliente.
Guarda información del cliente, incluyendo el nombre, dirección y un identificador único.

## Mensajería

### Cliente

Puede enviar mensajes de texto normales y comandos especiales como la transferencia de archivos.
Para enviar un mensaje de texto, simplemente escribe el mensaje que desea enviar.

### Servidor

Reenvía los mensajes de texto recibidos a todos los demás clientes conectados.
Procesa comandos especiales como solicitudes de transferencia de archivos.

## Transferencia de Archivos

- El cliente inicia la transferencia enviando un comando especial:

  > file: nombre_del_archivo

- Envía la información del archivo:
- El servidor detecta que el cliente desea enviar un archivo (mensaje comenzado con “file: <nombre_del_archivo>”)

  > Aquí comienza el proceso de recepción del archivo de parte del servidor.

  > Después de recibir la info del archivo, el servidor envía una señal para indicarle al cliente que comience a enviar el contenido del archivo (“sr”)

  > Cuando el cliente recibe la señal “sr” comienza el envio del contenido.

- Cuando el servidor recibe la totalidad del archivo, comienza el envio al resto de los clientes:

  > Envía la información del archivo con el siguiente formato: SENDING_FILE nombre_del_archivo #tamaño_del_archivo

  > El cliente recibe la información y manda una señal (“ready”) para que el servidor comience a enviar el contenido del archivo

- Una vez que el cliente recibe la totalidad del archivo, imprime por pantalla un mensaje indicando que el archivo fue recibido de manera exitosa.

## Manejo de Errores y Desconexiones

### Cliente

Envía un mensaje de salida para desconectarse del servidor:

- exit

### Servidor

- Elimina al cliente de la lista de clientes conectados y notifica a los demás en caso de desconexión.
- Maneja errores en la transferencia de archivos y en la comunicación de mensajes

## Ejemplo de Secuencia de Mensajes

### Conexión y Mensajería

#### Cliente A se conecta:

> Cliente → Servidor: Alice

> Servidor → Todos los clientes: Alice has joined

#### Cliente A envía un mensaje:

> Cliente → Servidor: [Alice]: Hola a todos!

> Servidor → Todos los clientes: [Alice]: Hola a todos!

### Transferencia de Archivos

#### Cliente A quiere enviar un archivo:

> Cliente → Servidor: file: documento.pdf

> Cliente → Servidor: file: documento.pdf#12345 (tamaño del archivo en bytes)

> Servidor → Cliente A: sr

> Cliente A → Servidor: (envía datos del archivo en bloques de 1024 bytes)

#### Servidor notifica a los otros clientes:

> Servidor → Todos los clientes: SENDING_FILEdocumento.pdf#12345

#### Cliente B confirma estar listo para recibir:

> Cliente B → Servidor: ready

#### Servidor envía el archivo a Cliente B:

> Servidor → Cliente B: (envía datos del archivo en bloques de 1024 bytes)
