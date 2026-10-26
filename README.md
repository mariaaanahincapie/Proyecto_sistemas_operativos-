# Simulador de Evaluación de Exámenes Médicos

**Proyecto académico de sistemas concurrentes y paralelos**

| Información | Descripción |
|---|---|
| Lenguaje de programación | C++ |
| Sistemas operativos objetivo | Linux y Windows |
| Modalidad de trabajo | Equipo de dos integrantes |
| Estado | Diseño y especificación |
| Entrega actual | Primera entrega (10 %) |
| Documento de referencia | Simulación de un sistema de evaluación de exámenes médico |

## 1. Descripción del proyecto

El Simulador de Evaluación de Exámenes Médicos es un sistema concurrente diseñado para representar el funcionamiento de una infraestructura automatizada de análisis de muestras médicas.

El sistema recibe muestras, les asigna identificadores únicos, las organiza en colas de entrada y las distribuye a analizadores especializados según su tipo. Cada analizador procesa las muestras correspondientes, consume los reactivos necesarios y genera un resultado que se almacena en una cola de salida hasta que sea solicitado por el usuario.

La infraestructura contempla múltiples procesos de registro, colas de capacidad limitada, hilos concurrentes de clasificación y procesamiento, memoria compartida y un único robot encargado de gestionar las recargas de reactivos.

El proyecto busca aplicar conceptos de programación concurrente, sincronización, comunicación entre procesos, administración de recursos compartidos y diseño de software multiplataforma.

**Importante:** este proyecto es una simulación académica. Los resultados son generados aleatoriamente y no tienen validez clínica ni sirven para realizar diagnósticos médicos reales.

## 2. Objetivos

### 2.1. Objetivo general

Diseñar e implementar un sistema concurrente que simule la recepción, clasificación, procesamiento y reporte de muestras médicas, controlando la capacidad de las colas, la disponibilidad de los analizadores y el acceso concurrente a los recursos compartidos.

### 2.2. Objetivos específicos

- Diseñar una interfaz de línea de comandos para inicializar, registrar, controlar y reportar el estado del sistema.
- Definir las estructuras de datos de las colas de entrada, las colas internas de procesamiento y la cola de salida.
- Implementar mecanismos de sincronización para coordinar procesos e hilos.
- Administrar analizadores especializados según el tipo de muestra.
- Simular el consumo, la disponibilidad y la recarga de reactivos.
- Garantizar la asignación de identificadores únicos a las muestras.
- Definir el comportamiento de las muestras durante las distintas etapas de procesamiento.
- Diseñar una arquitectura compatible con Linux y Windows.
- Documentar las decisiones técnicas antes de iniciar la implementación.

## 3. Alcance funcional

El sistema contempla las siguientes funcionalidades:

1. Inicializar los recursos y configurar los parámetros del sistema.
2. Registrar muestras de forma interactiva o mediante archivos.
3. Asignar un identificador único a cada muestra aceptada.
4. Gestionar múltiples colas de entrada con capacidad limitada.
5. Clasificar y distribuir las muestras según su tipo.
6. Procesar muestras de manera concurrente mediante analizadores especializados.
7. Gestionar los niveles de reactivos y coordinar su recarga.
8. Consultar muestras en espera, muestras en procesamiento, resultados pendientes y niveles de reactivos.
9. Actualizar los niveles de reactivos mediante comandos de control.
10. Obtener resultados mediante reportes interactivos o por cantidad solicitada.

El comportamiento general se basa en la guía de la práctica. Los aspectos que no están definidos completamente se especificarán durante la fase de diseño.

## 4. Arquitectura general

La guía establece cinco componentes principales: Registro, Entrada, Evaluador, Salida y Reporte. Estos componentes se complementan con los analizadores especializados, la gestión de reactivos y los mecanismos de sincronización.

### 4.1. Interfaz principal

La interfaz del sistema se denominará `evaluador` y ofrecerá cuatro comandos:

| Comando | Responsabilidad |
|---|---|
| `evaluador init` | Inicializar el sistema y configurar sus recursos. |
| `evaluador reg` | Registrar muestras y obtener sus identificadores. |
| `evaluador ctrl` | Consultar estados y actualizar los niveles de reactivos. |
| `evaluador rep` | Solicitar y retirar resultados procesados. |

La guía define estos comandos como la interfaz principal. Su sintaxis, parámetros, validaciones y respuestas se detallarán en la documentación técnica.

### 4.2. Registro

El componente de registro recibe las solicitudes de nuevas muestras desde la entrada estándar o desde archivos.

Cada registro especifica la cola de entrada, el tipo de muestra y la cantidad correspondiente. Una vez validado, el sistema asigna un identificador único y solicita espacio en la cola indicada.

La guía permite ejecutar varias instancias del comando `reg`. Cuando una cola está llena, el registro debe esperar hasta que se libere una posición.

### 4.3. Entrada y clasificación

El componente de entrada administra las colas donde se almacenan temporalmente las muestras pendientes.

La arquitectura contempla una cantidad configurable de colas de entrada y un hilo consumidor por cada cola. Estos hilos extraen las muestras y las distribuyen a las colas internas correspondientes.

Las colas tendrán una capacidad limitada para controlar el uso de memoria y coordinar la producción y el consumo de muestras.

### 4.4. Evaluador y analizadores

El evaluador coordina los hilos de entrada y los hilos de procesamiento. Según la guía, existirá un hilo de procesamiento por cada tipo de muestra.

| Código | Tipo de muestra | Tiempo de procesamiento |
|---|---|---|
| `B` | Sangre | Entre 1 y 7 segundos |
| `D` | Detritos | Entre 5 y 20 segundos |
| `S` | Piel | Entre 8 y 25 segundos |

El tiempo de procesamiento se seleccionará aleatoriamente dentro del intervalo correspondiente.

Cada analizador atenderá exclusivamente las muestras del tipo para el que está especializado y deberá disponer del reactivo necesario para procesarlas.

### 4.5. Gestión de reactivos

Cada tipo de muestra tiene asociado un recurso de reactivo. Cuando un analizador necesite una recarga, deberá coordinarse con el único robot de transporte definido en la guía.

El robot tiene una capacidad limitada y solo puede transportar un tipo de reactivo por viaje. Mientras se realiza una recarga, el analizador correspondiente no puede procesar otra muestra.

La cantidad transportada, el consumo por muestra, la duración de las recargas y el mecanismo de solicitud se definirán en la documentación de diseño.

### 4.6. Salida y reporte

El componente de salida almacena los resultados generados por los analizadores en una cola de capacidad limitada.

El sistema debe impedir que se supere la capacidad de la cola y debe conservar el orden de entrega establecido en la especificación.

El componente de reporte permitirá obtener resultados mediante dos modalidades: una asociada a un intervalo de espera y otra que espera hasta completar la cantidad de resultados solicitada.

La política exacta del orden de salida se definirá como una decisión de diseño, dado que la guía exige mantener un orden, pero no especifica inequívocamente si corresponde al orden de registro o al orden de finalización de los análisis.

## 5. Flujo de funcionamiento

El flujo previsto del sistema comprende las siguientes etapas:

1. **Inicialización:** se configuran las capacidades de las colas, los niveles iniciales de reactivos y los recursos compartidos.
2. **Registro:** uno o varios procesos registran muestras mediante la entrada interactiva o archivos.
3. **Almacenamiento:** cada muestra aceptada recibe un identificador único y se incorpora a la cola de entrada correspondiente.
4. **Clasificación:** los hilos de entrada extraen las muestras y las distribuyen según su tipo.
5. **Procesamiento:** los hilos especializados procesan las muestras, siempre que exista disponibilidad del analizador y de los reactivos.
6. **Recarga:** cuando un analizador requiere reactivos, solicita el servicio del robot único.
7. **Publicación:** al finalizar el procesamiento, el resultado se incorpora a la cola de salida cuando existe capacidad.
8. **Reporte:** el usuario consulta y retira los resultados mediante el comando `rep`.

Las operaciones de registro, clasificación, procesamiento, control y reporte podrán coincidir temporalmente. Por ello, las estructuras compartidas y las colas deberán contar con mecanismos adecuados de sincronización.

## 6. Modelo de concurrencia y recursos compartidos

El sistema utilizará un modelo concurrente basado en procesos externos, hilos internos y estructuras de datos compartidas.

### 6.1. Memoria compartida

La guía establece que las colas de entrada y salida deben almacenarse en una única región de memoria compartida, junto con las variables auxiliares necesarias para controlar el sistema.

Esta memoria permitirá que las distintas instancias de los comandos accedan al estado común de la aplicación.

La representación de los datos y los mecanismos de acceso deberán ser compatibles con Linux y Windows.

### 6.2. Modelo productor-consumidor

El sistema contempla los siguientes flujos:

- Los procesos de registro producen muestras.
- Los hilos de entrada consumen muestras de las colas de entrada.
- Los hilos de entrada producen elementos en las colas internas.
- Los hilos especializados consumen muestras de sus colas internas.
- Los analizadores producen resultados.
- El componente de reporte consume los resultados disponibles.

La sincronización deberá impedir accesos incompatibles, pérdida de muestras, duplicación de identificadores y desbordamiento de las colas.

### 6.3. Mecanismos de sincronización

Se evaluará el uso de mutex, semáforos, variables de condición y operaciones atómicas, seleccionando los mecanismos apropiados para cada recurso.

La sincronización entre procesos deberá ser compatible con la plataforma. Los mutex convencionales de C++ no se asumirán suficientes para sincronizar por sí solos procesos independientes.

La documentación técnica especificará las operaciones que requieren exclusión mutua, las condiciones de espera y las medidas para prevenir bloqueos indefinidos.

## 7. Parámetros de configuración

La guía establece los siguientes valores predeterminados para la inicialización:

| Parámetro | Descripción | Valor predeterminado |
|---|---|---:|
| `-i` | Número de colas de entrada | 5 |
| `-ie` | Capacidad por cola de entrada | 6 |
| `-oe` | Capacidad de la cola de salida | 10 |
| `-n` | Nombre del segmento de memoria compartida | `evaluador` |
| `-b` | Nivel inicial de reactivo de sangre | 100 |
| `-d` | Nivel inicial de reactivo de detritos | 100 |
| `-s` | Nivel inicial de reactivo de piel | 100 |
| `-q` | Capacidad de las colas internas | 6 |
| `-ee` | Parámetro presente en la sintaxis | Pendiente de aclaración |

El significado de `-ee` no está explicado en la descripción de parámetros de la guía y deberá consultarse con el docente antes de completar la especificación.

Las validaciones, los límites permitidos y el comportamiento ante configuraciones inválidas se definirán en el diseño de la interfaz.

## 8. Generación de resultados

La guía establece la siguiente distribución aleatoria para los resultados:

| Resultado | Código | Probabilidad |
|---|---|---:|
| Positivo | `P` | 50 % |
| Negativo | `N` | 35 % |
| No concluyente | `?` | 15 % |

El simulador generará los resultados de acuerdo con estas probabilidades.

Un resultado no concluyente indica que el examen debe repetirse dentro del modelo descrito por la práctica. La eventual repetición automática y su impacto en las colas deberán definirse si se decide incorporarlas.

Los resultados son exclusivamente simulados y no tienen validez clínica.

## 9. Tecnologías y compatibilidad

### 9.1. Lenguaje de programación

Se ha seleccionado C++ como lenguaje previsto para la implementación, dentro de las alternativas permitidas por la guía.

Se propone utilizar C++17 como estándar inicial, sujeto a la validación de las necesidades técnicas y de las herramientas seleccionadas.

### 9.2. Sistemas operativos

El proyecto se diseñará para Linux y Windows, de acuerdo con el requisito de compatibilidad para trabajos en equipo establecido en la guía.

Se propone una capa de abstracción para encapsular las operaciones dependientes del sistema operativo, manteniendo común la lógica de registro, procesamiento, colas y reportes.

### 9.3. Herramientas de desarrollo

Las herramientas previstas son:

- Git para el control de versiones.
- Un compilador compatible con C++17.
- CMake como sistema de construcción multiplataforma.
- Las herramientas de construcción requeridas por el compilador seleccionado.

El uso de CMake es una decisión propuesta por el equipo. Las dependencias adicionales y sus versiones se definirán antes de la implementación.

## 10. Instalación y preparación del entorno

Esta sección describe cómo preparar las herramientas necesarias para trabajar con el proyecto.

**Nota:** los comandos de esta sección preparan el entorno. La compilación y ejecución funcionales estarán disponibles cuando se haya implementado el código y se hayan creado los archivos de construcción correspondientes.

### 10.1. Requisitos previos

| Herramienta | Propósito |
|---|---|
| Git | Descargar el repositorio y gestionar versiones. |
| Compilador de C++ | Compilar el código fuente. |
| CMake | Configurar la construcción del proyecto. |
| Terminal | Ejecutar comandos de instalación, compilación y operación. |

### 10.2. Instalación en Linux

En sistemas basados en Ubuntu o Debian, se propone instalar las herramientas mediante:

```bash
sudo apt update
sudo apt install -y git g++ cmake make
```

Verificación de las herramientas:

```bash
git --version
g++ --version
cmake --version
make --version
```

Estos comandos permiten comprobar que las herramientas básicas están disponibles.

En otras distribuciones de Linux, los nombres de los paquetes y el gestor de paquetes pueden variar.

### 10.3. Instalación en Windows

En Windows se propone utilizar PowerShell y un entorno de compilación de C++ compatible con CMake.

Herramientas previstas:

- Git para Windows.
- CMake.
- Visual Studio Build Tools con MSVC, o MinGW-w64.
- Un generador de construcción compatible con el compilador elegido.

Después de la instalación, se podrá verificar Git y CMake desde PowerShell:

```powershell
git --version
cmake --version
```

Si se utiliza MSVC, la comprobación del compilador se realizará desde una terminal configurada para ese entorno:

```powershell
cl
```

Si se utiliza MinGW-w64:

```powershell
g++ --version
```

El compilador y el generador seleccionados deberán mantenerse coherentes durante la configuración y la construcción del proyecto.

### 10.4. Obtener el repositorio

Una vez creado el repositorio en GitHub, se podrá descargar mediante:

```bash
git clone <URL_DEL_REPOSITORIO>
cd <DIRECTORIO_DEL_PROYECTO>
```

Los valores entre corchetes angulares deberán sustituirse por la URL real y el nombre del directorio del repositorio.

### 10.5. Dependencias

Se priorizará el uso de la biblioteca estándar de C++ y de los mecanismos necesarios para implementar memoria compartida y sincronización.

Toda dependencia adicional deberá evaluarse según su compatibilidad con Linux y Windows.

La lista definitiva de bibliotecas, versiones y procedimientos de instalación se actualizará cuando se cierre el diseño técnico.

## 11. Construcción y ejecución prevista

El sistema ofrecerá una interfaz de línea de comandos mediante el ejecutable `evaluador`.

El flujo previsto comprende la compilación, inicialización, registro de muestras, consulta de estados y obtención de resultados.

**Los comandos siguientes representan la interfaz prevista y no garantizan una ejecución funcional en la fase actual de diseño.**

### 11.1. Compilación

Se propone utilizar CMake para organizar la construcción multiplataforma.

Una vez creado y configurado el archivo `CMakeLists.txt`, la construcción prevista será:

```bash
cmake -S . -B build
cmake --build build
```

El primer comando configura el directorio de construcción y el segundo compila el proyecto.

La ubicación final del ejecutable dependerá de la configuración del proyecto y del generador utilizado.

### 11.2. Inicialización

El comando `init` preparará los recursos necesarios para el funcionamiento del sistema.

Ejemplo conceptual:

```bash
evaluador init
```

Ejemplo con parámetros:

```bash
evaluador init -i 5 -ie 6 -oe 10 -b 100 -d 100 -s 100 -q 6
```

El ejemplo utiliza valores predeterminados de la guía. La sintaxis definitiva deberá contemplar el parámetro `-ee` cuando se aclare su significado.

### 11.3. Registro de muestras

El sistema permitirá registrar muestras de forma interactiva o desde archivos.

Registro interactivo:

```bash
evaluador reg -
```

Registro desde archivo:

```bash
evaluador reg examples/muestras.txt
```

Formato de entrada establecido por la guía:

```text
<cola_de_entrada> <tipo_de_muestra> <cantidad>
```

Ejemplo de archivo:

```text
0 B 2
1 D 3
2 S 1
```

Los registros representan muestras de sangre, detritos y piel. La cola debe encontrarse dentro del intervalo configurado y la cantidad debe estar entre 1 y 5.

### 11.4. Consulta y control

El comando `ctrl` permitirá consultar el estado del sistema y actualizar los niveles de reactivos.

Ejemplo:

```bash
evaluador ctrl
```

Dentro del modo interactivo se podrán utilizar las siguientes operaciones:

```text
list waiting
list processing
list reported
list reactive
list all
```

Para actualizar reactivos:

```text
update B 20
update D 15
update S 10
```

Estos ejemplos representan la interfaz descrita en la guía. La validación de entradas y el formato completo de las respuestas se especificarán antes de la implementación.

### 11.5. Obtención de resultados

El comando `rep` permitirá solicitar resultados mediante las dos modalidades previstas en la guía.

Ejemplo de reporte asociado a un intervalo de espera:

```bash
evaluador rep -i 10
```

Ejemplo de reporte por cantidad:

```bash
evaluador rep -m 5
```

El primer ejemplo representa una solicitud asociada a diez segundos de espera; el segundo solicita cinco resultados y espera hasta completar la cantidad indicada.

La semántica exacta de las esperas, la terminación de las solicitudes y el orden de entrega se definirá en la especificación del comando.

### 11.6. Secuencia de ejecución

El orden previsto de utilización será:

1. Instalar las herramientas de desarrollo.
2. Obtener el repositorio.
3. Configurar y compilar el proyecto.
4. Inicializar el sistema con `evaluador init`.
5. Registrar muestras con `evaluador reg`.
6. Consultar estados y administrar reactivos con `evaluador ctrl`.
7. Obtener resultados con `evaluador rep`.
8. Finalizar el sistema mediante el mecanismo de terminación que se defina en el diseño.

La inicialización deberá preparar los recursos compartidos antes de admitir operaciones de registro o consulta.

La guía indica que `init` inicia el evaluador y ocupa la consola desde la que se ejecuta. Por tanto, la interacción entre ese proceso y los demás comandos deberá respetar el ciclo de vida definido en la práctica.

El mecanismo de apagado ordenado todavía debe especificarse.


## 12. Integrantes

- Integrante 1: Mariana Hincapié Henao 
- Integrante 2: 


