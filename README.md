# clisynth: Sintetizador de terminal
Otro proyecto mas que hago por mero aburrimiento  
Un sintetizador de cli funcional escrito en C que anda medio en demo (le faltan efectitos) pero que siento yo va muy bien encaminado  
Tiene soporte para volumen, octavado, vibrato, transposicion y 4 formas de onda diferentes  
Aunque esta medio flojo de papeles (algun bug creo q tiene q ire arreglando) esta facha, pa boludear esta bueno  

## Dependencias
Necesita 3 simples cositas para estar en funcionamiento:
* `make`: para montado e instalacion
* `gcc`: para compilar el codigo
* `aplay`: para la reproduccion de ondas

## ¿Como lo uso?
Las instrucciones son simplitas
* **b** y **v** para subir y bajar volumen
* **+** y **-** para subir y bajar de octava
* **1 a 4** para cambiar tipo de onda
* **rueda de mouse** para modular vibrato
* **0** y **9** para transposicion  
Las notas son:  
Arriba la nota  
Abajo con que ejecutarla
```
┌──┬──┬┬──┬─┬──┬──┬┬──┬┬──┬─┬───┐  
│  │C#││D#│ │  │F#││G#││A#│ │   │  
│  │W ││E │ │  │T ││Y ││U │ │   │  
│  └┬─┘└┬─┘ │  └┬─┘└┬─┘└┬─┘ │   │  
│C  │D  │E  │F  │G  │A  │B  │C  │  
│A  │S  │D  │F  │G  │H  │J  │K  │  
└───┴───┴───┴───┴───┴───┴───┴───┘
```

## Y como lo instalo?
Solo pone:  
` make && sudo make install `  
para instalarlo globalmente.  
Para desinstalarlo:  
`sudo make uninstall`  

## Recomendaciones
Si me queres recomendar algo, bienvenido seas! Abrite una pull request o reporta en Issues
