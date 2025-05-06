# PNG-2LPTic

The original of [PNG-2LPTic](https://github.com/SiyiZhao/PNG-2LPTic) is the [2LPTnonlocal code](https://cosmo.nyu.edu/roman/2LPT/) from Roman Scoccimarro.

Updated with the version used in Quijote simulations (https://github.com/dsjamieson/2LPTPNG) with following things:
- Enable FixedAmplitude and PhaseFlip;
- Add ns!=1 capability for all PNG cases;
- Change `drfftw` to `rfftw` since double precision is not needed.