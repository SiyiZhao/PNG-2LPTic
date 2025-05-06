# PNG-2LPTic

The original of [PNG-2LPTic](https://github.com/SiyiZhao/PNG-2LPTic) is the [2LPTnonlocal code](https://cosmo.nyu.edu/roman/2LPT/) from Roman Scoccimarro.

Updated with the version used in Quijote simulations (https://github.com/dsjamieson/2LPTPNG) with following things:
- Enable FixedAmplitude and PhaseFlip;
- Add ns!=1 capability for all PNG cases;
- Change `drfftw` to `rfftw` since double precision is not needed;
- Delete `FnlTime`, `DstartFnl`, `RedshiftFnl`, since they should be in $z=0$ (important change);
- Add timing of the code.

There are also some changes appeared in the Quijote version but I do not adopt: 
- They delete some *information printing*, I keep them in this version.
- They set up `Makefile` with system specific options ("flatiron" & "GordonS"), I keep the original `Makefile` in this version.