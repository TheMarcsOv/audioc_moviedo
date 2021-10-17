# Código para la parte de 'Transporte Multimedia' de SMA

Descomprima este fichero en la máquina virtual o su cuenta del laboratorio (`unzip`, si lo hace dentro de la máquina virtual) 

El código está organizado de la siguiente forma:

`pr0/`
	Código correspondiente a parte de la práctica 0, cuyo objetivo es familiarizarse con la gestión de memoria de C y las facilidades básicas de los sockets.

- `fail_mem.c` : código con errores en el uso de memoria que hay que descubrir  
- `mcast_example_...`: dos ficheros que permiten establecer una conexión multicast entre dos equipos situados en la misma LAN

`lib/`
	Librerías que pueden utilizar las aplicaciones de audio: configuración de la tarjeta de sonido, buffer circular y ficheros de declaraciones de tipos.

`audioSimple/`
	Código que graba de la tarjeta de sonido y vuelca a un fichero, y que lee del fichero y reproduce en la tarjeta de sonido.  Utiliza herramientas de `lib/`.  
	Este código ya está completo. Puede leerlo para entender cómo funciona, y ejecutarlo para observar los resultados.

`audioc/`
	Material básico para empezar el proyecto `audioc`, para la evaluación de la parte de 'transporte multimedia'. Contiene un esqueleto del código, y funciones para la captura de argumentos. 

`utilities/`

- `diffTime`: utilidad para analizar más cómodamente los resultados de las trazas de strace. Se explica su uso en el documento 'Testing conf'
- `test_getodelay` y `test_select_sndcard_timing`: tanto esta utilidad como la siguiente permiten probar dos funcionalidades de la API de la tarjeta de sonido. El primero, el correcto retorno del número de bytes pendientes por reproducir en la tarjeta. El segundo, comprobar que los bloqueos en el acceso a la tarjeta de sonido se realizan en los momentos correctos. Un motivo por el que esto podría no ser así es algún problema en el soporte de sonido del VirtualBox de su máquina (configuración incorrecta, algún problema con la versión de virtualbox, etc.). Por otro lado, también pueden servir para entender cómo funciona la tarjeta de sonido de forma completa. Las instrucciones sobre cómo ejecutarlo están el el propio código.
