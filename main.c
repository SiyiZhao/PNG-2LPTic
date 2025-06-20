#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include <complex.h> 
#include <gsl/gsl_rng.h>
#include "allvars.h"
#include "proto.h"

#define ASSERT_ALLOC(cond)                                                                     \
  {                                                                                            \
    if (!cond)                                                                                 \
    {                                                                                          \
      printf("failed to allocate %g Mbyte on Task %d\n", bytes / (1024.0 * 1024.0), ThisTask); \
      printf("bailing out.\n");                                                                \
      FatalError(1);                                                                           \
    }                                                                                          \
  }

int frequency_of_primes(int n)
{
  int i, j;
  int freq = n - 1;
  for (i = 2; i <= n; ++i)
    for (j = sqrt(i); j > 1; --j)
      if (i % j == 0)
      {
        --freq;
        break;
      }
  return freq;
}

void print_timed_done(int n)
{
  clock_t tot_time = clock() - start_time;
  int tot_hours = (int)floor(((double)tot_time) / 60. / 60. / CLOCKS_PER_SEC);
  int tot_mins = (int)floor(((double)tot_time) / 60. / CLOCKS_PER_SEC) - 60 * tot_hours;
  double tot_secs = (((double)tot_time) / CLOCKS_PER_SEC) - 60. * (((double)tot_mins) * 60. * ((double)tot_hours));
  clock_t diff_time = clock() - previous_time;
  int diff_hours = (int)floor(((double)diff_time) / 60. / 60. / CLOCKS_PER_SEC);
  int diff_mins = (int)floor(((double)diff_time) / 60. / CLOCKS_PER_SEC) - 60 * diff_hours;
  double diff_secs = (((double)diff_time) / CLOCKS_PER_SEC) - 60. * (((double)diff_mins) * 60. * ((double)diff_hours));
  for (int i = 0; i < n; i++)
    printf(" ");
  printf("Done [%02d:%02d:%05.2f, %02d:%02d:%05.2f]\n", diff_hours, diff_mins, diff_secs, tot_hours, tot_mins, tot_secs);
  previous_time = clock();
  return;
}



int main(int argc, char **argv)
{

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &ThisTask);
  MPI_Comm_size(MPI_COMM_WORLD, &NTask);

  start_time = clock();
  previous_time = start_time;

  if (argc < 2)
  {
    if (ThisTask == 0)
    {
      fprintf(stdout, "\nParameters are missing.\n");
      fprintf(stdout, "Call with <ParameterFile>\n\n");
    }
    MPI_Finalize();
    exit(0);
  }

  read_parameterfile(argv[1]);
  checkchoose();
  set_units();
  initialize_transferfunction();
  initialize_powerspectrum();
  initialize_ffts();
  read_glass(GlassFile);

  if (ThisTask == 0)
    print_setup();

  displacement_fields();

  if (ThisTask == 0)
  {
    printf("Writing initial conditions snapshot...");
    fflush(stdout);
  };
  write_particle_data();
  if (ThisTask == 0)
    print_timed_done(10);
  if (NumPart)
    free(P);
  free_ffts();
  MPI_Barrier(MPI_COMM_WORLD);
  print_spec();
  MPI_Finalize(); /* clean up & finalize MPI */
  exit(0);
}

void print_setup(void)
{
  char pstr[79];

  for (int i = 0; i < 79; i++)
    pstr[i] = '*';
  printf("%s\n", pstr);

#ifdef ONLY_GAUSSIAN
  char exec[] = "  2LPT  ";
#endif
#ifdef LOCAL_FNL
  char exec[] = "2LPTNGLC";
#endif
#ifdef EQUIL_FNL
  char exec[] = "2LPTNGEQ";
#endif
#ifdef ORTOG_FNL
  char exec[] = "2LPTNGOR";
#endif
// wrc
#ifdef ORTOG_LSS_FNL
  char exec[] = "2LPTNGORLSS";
#endif
// *** Collider Addition (Start) ***
// This sets the mode for IC generation specified in the Makefile
#ifdef QSFI_FNL
  char exec[] = "2LPTNGQSFI";
#endif
#ifdef OSC_FNL
  char exec[] = "2LPTNGOSC";
#endif
// *** Collider Addition (End) ***

  pstr[0] = '*';
  for (int i = 1; i < 79 / 2 - 3; i++)
    pstr[i] = ' ';
  sprintf(&pstr[79 / 2 - 4], "%s", exec);
  for (int i = 79 / 2 + 4; i < 78; i++)
    pstr[i] = ' ';
  pstr[78] = '*';
  printf("%s\n", pstr);
  for (int i = 0; i < 79; i++)
    pstr[i] = '*';
  printf("%s\n", pstr);

  printf("\n");

  printf(" Box = %.2e   Nmesh = %d   Nsample = %d\n", Box, Nmesh, Nsample);
  printf(" Nglass = %d    GlassTileFac = %d\n\n", Nglass, GlassTileFac);
  printf(" Omega = %.4f         OmegaLambda = %.4f    OmegaBaryon = %.2e\n", Omega, OmegaLambda, OmegaBaryon);
  printf(" sigma8 = %.4f     Anorm = %.3e     PrimoridalIndex = %.4f       Redshift = %.2e\n", Sigma8, Anorm, PrimordialIndex, Redshift);
  printf(" HubbleParam = %.4f  OmegaDM_2ndSpecies = %.2e    fNL = %+.2e    Delta = %+.2e\n", HubbleParam, OmegaDM_2ndSpecies, Fnl, Delta);
  printf(" Klong_max = %+.2e  Spin = %d    Nu = %+.2e    Phase = %+.2e\n", Klong_max, Spin, Nu, Phase);
  printf(" FixedAmplitude = %d    PhaseFlip = % d   SphereMode = %d    Seed = %d\n", FixedAmplitude, PhaseFlip, SphereMode, Seed);
  printf("***************************************************************************************************\n");
  // Write code to check if OSC_FNL and Delta!=1.5. If so, throw a warning because cosmo collider template is motivated for Delta=1.5. ALthough code will run for bothcases
  #ifdef OSC_FNL
    if (Delta != 1.5){
      printf(" WARNING: you are running oscillatory mode with Delta = %+.2e. This will not produce an error,\n but the physically motivated collider template has Delta=1.5.\n", Delta);
    }
    printf("***************************************************************************************************\n");
  #endif


  for (int i = 0; i < 79; i++)
    pstr[i] = '*';
  printf("\n%s\n\n", pstr);
  return;
}

void displacement_fields(void){
  MPI_Request request;
  MPI_Status status;
  gsl_rng *random_generator;
  int i, j, k, ii, jj, kk, i_herm, j_herm, k_herm, axes;
  int n;
  int sendTask, recvTask;
  #ifdef ONLY_GAUSSIAN
    double p_of_k, delta;
  #else
  // Initialize variables relevant for local FNL (and QSFI)
  double t_of_k, phig, Beta, twb;
  fftw_complex *(cpot); /* For computing nongaussian fnl ic */
  fftw_real *(pot);

  #ifdef OUTPUT_DF
    fftw_real *(pot_global);
  #endif


  // *** Collider Addition (Start) ***
  #ifdef QSFI_FNL
    // |k|^Delta/3*(4-ns)~|k|^Delta for scale invariant
    double kmag_Delta_QSFI;
    
    // Define the k^Delta phi_G(k) field and its Fourier transform
    fftw_complex *(ckdeltaphi);
    fftw_real *(kdeltaphi);

    // Define the auxiliary field, Psi, which is computed from kdeltaphi
    fftw_complex *(cpsi);
    fftw_real *(psi);

    // Define squared Gaussian potential which must be subtracted to construct
    // psi field. Note that we create a new field for this because we need to 
    // apply the high-pass filter in Fourier space on IFFT[pot^2](k).
    fftw_complex *(cpot_sq);
    fftw_real *(pot_sq);

  #else
  #ifdef OSC_FNL
    // Define momentum powers |k|^{0.5*(4-ns)+i\nu}~|k|^3/2+i\nu for scale 
    // invariant
    double complex kmag_Delta_plus_inu; 
    double complex kmag_Delta_min_inu;

    // Define phase coefficients e^{i delta}
    double complex exp_plus_iphase;
    double complex exp_min_iphase;

    // Define some temporary complex numbers used for manipulations
    double complex temp_ck_coord, temp_ck_herm, temp_real, temp_imag, temp_complex, temp_cpsi_plus, temp_cpsi_min;



    // Define the k^{3/2+/-i\nu}phi_G(k) field and its Fourier transform. Since
    // we are working with a complex field, we need to define two fields for
    // the real and imaginary parts. 

    // Define the +i\nu field
    // fftw_complex *(ck_Delta_plus_inu_full); // k^{Delta+i\nu} (fully complex FFT)
    fftw_complex *(ck_Delta_plus_inu_phi_full); // k^{Delta+i\nu}phi(k) (fully complex FFT)

    fftw_complex *(ck_Delta_plus_inu_phi_real);
    fftw_real *(k_Delta_plus_inu_phi_real);    
    fftw_complex *(ck_Delta_plus_inu_phi_imag); 
    fftw_real *(k_Delta_plus_inu_phi_imag);   
  
    // Define the -i\nu field
    // fftw_complex *(ck_Delta_min_inu_full); // k^{Delta-i\nu} (fully complex FFT)
    fftw_complex *(ck_Delta_min_inu_phi_full); // k^{Delta-i\nu}phi(k) (fully complex FFT)
  
    fftw_complex *(ck_Delta_min_inu_phi_real); 
    fftw_real *(k_Delta_min_inu_phi_real);    
    fftw_complex *(ck_Delta_min_inu_phi_imag); 
    fftw_real *(k_Delta_min_inu_phi_imag);   
  
    // Define the auxiliary field, Psi=Psi_plus+Psi_minus
    fftw_complex *(cpsi);
    fftw_real *(psi);


    // Define squared Gaussian potential which must be subtracted to construct
    // psi field.
    fftw_complex *(cpot_sq);
    fftw_real *(pot_sq);
    
    // Define receive fields for MPI communication (used for Hermitian indices)
    fftw_complex *(cpot_received);
    fftw_complex *(ck_Delta_plus_inu_phi_full_received);
    fftw_complex *(ck_Delta_min_inu_phi_full_received);
    
  #else
  // *** Collider Addition (END) ***
  #ifndef LOCAL_FNL
    if (ThisTask == 0)
    {
      printf("Defining quantities for non-local non-collider PNG\n");
      fflush(stdout);
    };
    // *************************** DSJ *******************************
    double exp_1_over_3, exp_1_over_6, kmag_1_over_3, kmag_2_over_3;
    // *************************** DSJ *******************************
    fftw_complex *(cpartpot); /* For non-local fluctuations */
    fftw_real *(partpot);
    fftw_complex *(cp1p2p3sym);
    fftw_real *(p1p2p3sym);
    fftw_complex *(cp1p2p3sca);
    fftw_real *(p1p2p3sca);
    fftw_complex *(cp1p2p3nab);
    fftw_real *(p1p2p3nab);
    fftw_complex *(cp1p2p3tre);
    fftw_real *(p1p2p3tre);
  // ****  wrc ****
  #ifdef ORTOG_LSS_FNL
    // Will contain phi_G/k (hence invs)
    fftw_complex *(cp1p2p3inv);
    fftw_real *(p1p2p3inv);
    // names correspond to kernels in Appendix A of Coulton et al.
    fftw_complex *(cp1p2p3_K12D);
    fftw_real *(p1p2p3_K12D);
    fftw_complex *(cp1p2p3_K12E);
    fftw_real *(p1p2p3_K12E);
    fftw_complex *(cp1p2p3_K12F);
    fftw_real *(p1p2p3_K12F);
    fftw_complex *(cp1p2p3_K12G);
    fftw_real *(p1p2p3_K12G);
    double orth_p = 27.0 / (-21.0 + 743.0 / (7 * (20.0 * pow(PI, 2.0) - 193.0)));
    double orth_t = (2.0 + 20.0 / 9.0 * orth_p) / (6 * (1.0 + 5.0 / 9.0 * orth_p));
  #endif
  // ****  wrc ****
  #endif // Addition
  #endif // Addition
  #endif
  #endif
  double fac, vel_prefac, vel_prefac2;
  double kvec[3], kmag, kmag2;
  double phase, ampl, hubble_a;
  double u, v, w;
  double f1, f2, f3, f4, f5, f6, f7, f8;
  double dis, dis2, maxdisp, max_disp_glob;
  unsigned int *seedtable;
  // ******* FAVN *****
  double phase_shift;
  // ******* FAVN *****

  unsigned int bytes, nmesh3;
  int coord_1d, coord, coord_herm; /* Used for converting 3D->1D index when accessing array elements. coord_herm is used to store index of Hermitian entry */
  fftw_complex *(cdisp[3]), *(cdisp2[3]); /* ZA and 2nd order displacements */
  fftw_real *(disp[3]), *(disp2[3]);

  fftw_complex *(cdigrad[6]);
  fftw_real *(digrad[6]);

  #ifdef CORRECT_CIC
    double fx, fy, fz, ff, smth;
  #endif

  hubble_a = Hubble * sqrt(Omega / pow(InitTime, 3) + (1 - Omega - OmegaLambda) / pow(InitTime, 2) + OmegaLambda);
  vel_prefac = InitTime * hubble_a * F_Omega(InitTime) / sqrt(InitTime);
  vel_prefac2 = InitTime * hubble_a * F2_Omega(InitTime) / sqrt(InitTime);

  // ******************************************** FAVN **********************************************
  phase_shift = 0.0;
  if (PhaseFlip == 1)
    phase_shift = PI;
  // ******************************************** FAVN **********************************************

  fac = pow(2 * PI / Box, 1.5);

  maxdisp = 0;

  random_generator = gsl_rng_alloc(gsl_rng_ranlxd1);

  gsl_rng_set(random_generator, Seed);

  if (!(seedtable = malloc(Nmesh * Nmesh * sizeof(unsigned int))))
    FatalError(4);

  for (i = 0; i < Nmesh / 2; i++)
  {
    for (j = 0; j < i; j++)
      seedtable[i * Nmesh + j] = 0x7fffffff * gsl_rng_uniform(random_generator);

    for (j = 0; j < i + 1; j++)
      seedtable[j * Nmesh + i] = 0x7fffffff * gsl_rng_uniform(random_generator);

    for (j = 0; j < i; j++)
      seedtable[(Nmesh - 1 - i) * Nmesh + j] = 0x7fffffff * gsl_rng_uniform(random_generator);

    for (j = 0; j < i + 1; j++)
      seedtable[(Nmesh - 1 - j) * Nmesh + i] = 0x7fffffff * gsl_rng_uniform(random_generator);

    for (j = 0; j < i; j++)
      seedtable[i * Nmesh + (Nmesh - 1 - j)] = 0x7fffffff * gsl_rng_uniform(random_generator);

    for (j = 0; j < i + 1; j++)
      seedtable[j * Nmesh + (Nmesh - 1 - i)] = 0x7fffffff * gsl_rng_uniform(random_generator);

    for (j = 0; j < i; j++)
      seedtable[(Nmesh - 1 - i) * Nmesh + (Nmesh - 1 - j)] = 0x7fffffff * gsl_rng_uniform(random_generator);

    for (j = 0; j < i + 1; j++)
      seedtable[(Nmesh - 1 - j) * Nmesh + (Nmesh - 1 - i)] = 0x7fffffff * gsl_rng_uniform(random_generator);
  }

  #ifdef ONLY_GAUSSIAN

    if (ThisTask == 0)
    {
      printf("Setting up gradient of Gaussian potential...");
      fflush(stdout);
    };

    for (axes = 0, bytes = 0; axes < 3; axes++)
    {
      cdisp[axes] = (fftw_complex *)malloc(bytes += sizeof(fftw_real) * TotalSizePlusAdditional);
      disp[axes] = (fftw_real *)cdisp[axes];
    }

    ASSERT_ALLOC(cdisp[0] && cdisp[1] && cdisp[2]);

  #if defined(MULTICOMPONENTGLASSFILE) && defined(DIFFERENT_TRANSFER_FUNC)
    for (Type = MinType; Type <= MaxType; Type++)
  #endif
  {

    /* first, clean the array */
    for (i = 0; i < Local_nx; i++)
      for (j = 0; j < Nmesh; j++)
        for (k = 0; k <= Nmesh / 2; k++)
          for (axes = 0; axes < 3; axes++)
          {
            cdisp[axes][(i * Nmesh + j) * (Nmesh / 2 + 1) + k].re = 0;
            cdisp[axes][(i * Nmesh + j) * (Nmesh / 2 + 1) + k].im = 0;

              /* ADDITION FOR OUTPUTTING MODES */
              #ifdef OUTPUT_DF
                //find the coordinates of the modes and put to 0 the modes
                //amplitudes and phases
                if (axes==0)
                  {
                    coord_DF[(i * Nmesh + j) * (Nmesh / 2 + 1) + k] = ((i+Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k;
                    amplitudes[(i * Nmesh + j) * (Nmesh / 2 + 1) + k] = 0;
                    phases[(i * Nmesh + j) * (Nmesh / 2 + 1) + k] = 0;
                  }
              #endif
              /* ADDITION FOR OUTPUTTING MODES */
          }

    for (i = 0; i < Nmesh; i++){
      ii = Nmesh - i;
      if (ii == Nmesh)
        ii = 0;
      if ((i >= Local_x_start && i < (Local_x_start + Local_nx)) ||
          (ii >= Local_x_start && ii < (Local_x_start + Local_nx)))
      {
        for (j = 0; j < Nmesh; j++)
        {
          gsl_rng_set(random_generator, seedtable[i * Nmesh + j]);

          for (k = 0; k < Nmesh / 2; k++)
          {
            phase = gsl_rng_uniform(random_generator) * 2 * PI;
            //************ FAVN ***************
            phase += phase_shift;
            //************ FAVN ***************
            do
              ampl = gsl_rng_uniform(random_generator);
            while (ampl == 0);

            if (i == Nmesh / 2 || j == Nmesh / 2 || k == Nmesh / 2)
              continue;
            if (i == 0 && j == 0 && k == 0)
              continue;

            if (i < Nmesh / 2)
              kvec[0] = i * 2 * PI / Box;
            else
              kvec[0] = -(Nmesh - i) * 2 * PI / Box;

            if (j < Nmesh / 2)
              kvec[1] = j * 2 * PI / Box;
            else
              kvec[1] = -(Nmesh - j) * 2 * PI / Box;

            if (k < Nmesh / 2)
              kvec[2] = k * 2 * PI / Box;
            else
              kvec[2] = -(Nmesh - k) * 2 * PI / Box;

            kmag2 = kvec[0] * kvec[0] + kvec[1] * kvec[1] + kvec[2] * kvec[2];
            kmag = sqrt(kmag2);

            if (SphereMode == 1)
            {
              if (kmag * Box / (2 * PI) > Nsample / 2) /* select a sphere in k-space */
                continue;
            }
            else
            {
              if (fabs(kvec[0]) * Box / (2 * PI) > Nsample / 2)
                continue;
              if (fabs(kvec[1]) * Box / (2 * PI) > Nsample / 2)
                continue;
              if (fabs(kvec[2]) * Box / (2 * PI) > Nsample / 2)
                continue;
            }

            p_of_k = PowerSpec(kmag);
            // ************ FAVN/DSJ ************
            if (!FixedAmplitude)
              p_of_k *= -log(ampl);
            // ************ FAVN/DSJ ************

            delta = fac * sqrt(p_of_k) / Dplus; /* scale back to starting redshift */

            if (k > 0)
            {
              if (i >= Local_x_start && i < (Local_x_start + Local_nx))
                for (axes = 0; axes < 3; axes++)
                {
                  cdisp[axes][((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k].re =
                      -kvec[axes] / kmag2 * delta * sin(phase);
                  cdisp[axes][((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k].im =
                      kvec[axes] / kmag2 * delta * cos(phase);

                  #ifdef OUTPUT_DF
                    if (axes==0)
                      {
                        amplitudes[((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k] = delta;
                        phases[((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k] = phase;
                      }
                  #endif
                }
            }
            else /* k=0 plane needs special treatment */
            {
              if (i == 0)
              {
                if (j >= Nmesh / 2)
                  continue;
                else
                {
                  if (i >= Local_x_start && i < (Local_x_start + Local_nx))
                  {
                    jj = Nmesh - j; /* note: j!=0 surely holds at this point */

                    for (axes = 0; axes < 3; axes++)
                    {
                      cdisp[axes][((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k].re =
                          -kvec[axes] / kmag2 * delta * sin(phase);
                      cdisp[axes][((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k].im =
                          kvec[axes] / kmag2 * delta * cos(phase);

                      cdisp[axes][((i - Local_x_start) * Nmesh + jj) * (Nmesh / 2 + 1) + k].re =
                          -kvec[axes] / kmag2 * delta * sin(phase);
                      cdisp[axes][((i - Local_x_start) * Nmesh + jj) * (Nmesh / 2 + 1) + k].im =
                          -kvec[axes] / kmag2 * delta * cos(phase);

                      #ifdef OUTPUT_DF
                        if (axes==0){
                          amplitudes[((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k] = delta;
                          phases[((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k] = phase;
                          amplitudes[((i - Local_x_start) * Nmesh + jj) * (Nmesh / 2 + 1) + k] = delta;
          					      phases[((i - Local_x_start) * Nmesh + jj) * (Nmesh / 2 + 1) + k] = -phase;
                        }
                      #endif
                    }
                  }
                }
              }
              else /* here comes i!=0 : conjugate can be on other processor! */
              {
                if (i >= Nmesh / 2)
                  continue;
                else
                {
                  ii = Nmesh - i;
                  if (ii == Nmesh)
                    ii = 0;
                  jj = Nmesh - j;
                  if (jj == Nmesh)
                    jj = 0;

                  if (i >= Local_x_start && i < (Local_x_start + Local_nx))
                    for (axes = 0; axes < 3; axes++)
                    {
                      cdisp[axes][((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k].re =
                          -kvec[axes] / kmag2 * delta * sin(phase);
                      cdisp[axes][((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k].im =
                          kvec[axes] / kmag2 * delta * cos(phase);

                      #ifdef OUTPUT_DF
                        if (axes==0){
                          amplitudes[((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k] = delta;
                          phases[((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k] = phase;
                        }
                      #endif
                    }

                  if (ii >= Local_x_start && ii < (Local_x_start + Local_nx))
                    for (axes = 0; axes < 3; axes++)
                    {
                      cdisp[axes][((ii - Local_x_start) * Nmesh + jj) * (Nmesh / 2 + 1) +
                                  k]
                          .re = -kvec[axes] / kmag2 * delta * sin(phase);
                      cdisp[axes][((ii - Local_x_start) * Nmesh + jj) * (Nmesh / 2 + 1) +
                                  k]
                          .im = -kvec[axes] / kmag2 * delta * cos(phase);
                      #ifdef OUTPUT_DF
          					  if (axes==0){
                        amplitudes[((ii - Local_x_start) * Nmesh + jj) * (Nmesh / 2 + 1) + k] = delta;
                        phases[((ii - Local_x_start) * Nmesh + jj) * (Nmesh / 2 + 1) + k] = -phase;
                      }
                      #endif					  
                    }
                }
              }
            }
          }
        }
      }
    }

    if (ThisTask == 0)
      print_timed_done(4);

    /* Addition for storing and saving output (DELETE THIS)*/
    MPI_Barrier(MPI_COMM_WORLD);
    #ifdef OUTPUT_DF
		if (ThisTask == 0){
			printf("\nWriting linear field.");
		}
			write_density_field_data();
		#endif
    

  #else /* non gaussian initial potential fnl type  */

  if (ThisTask == 0)
  {
    printf("Setting Gaussian potential for PNG...\n");
    fflush(stdout);
  };

  bytes = 0; /*initialize*/
  cpot = (fftw_complex *)malloc(bytes += sizeof(fftw_real) * TotalSizePlusAdditional);
  pot = (fftw_real *)cpot;

  ASSERT_ALLOC(cpot);

  /* first, clean the cpot array */
  for (i = 0; i < Local_nx; i++)
    for (j = 0; j < Nmesh; j++)
      for (k = 0; k <= Nmesh / 2; k++)
      {
        cpot[(i * Nmesh + j) * (Nmesh / 2 + 1) + k].re = 0;
        cpot[(i * Nmesh + j) * (Nmesh / 2 + 1) + k].im = 0;

        // Also clean linear field arrays if requested as output
        #ifdef OUTPUT_DF
          coord_DF[(i * Nmesh + j) * (Nmesh / 2 + 1) + k] = ((i+Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k;
          amplitudes[(i * Nmesh + j) * (Nmesh / 2 + 1) + k] = 0;
          phases[(i * Nmesh + j) * (Nmesh / 2 + 1) + k] = 0;
        #endif
      }

  /* Ho in units of UnitLength_in_cm and c=1, i.e., internal units so far  */
  /* Beta = 3/2 H(z)^2 a^3 Om(a) / D0 = 3/2 Ho^2 Om0 / D0 at redshift z = 0.0 */
  Beta = 1.5 * Omega / (2998. * 2998. / UnitLength_in_cm / UnitLength_in_cm * 3.085678e24 * 3.085678e24) / D0;

  for (i = 0; i < Nmesh; i++)
  {
    ii = Nmesh - i;
    if (ii == Nmesh)
      ii = 0;
    if ((i >= Local_x_start && i < (Local_x_start + Local_nx)) ||
        (ii >= Local_x_start && ii < (Local_x_start + Local_nx)))
    {
      for (j = 0; j < Nmesh; j++)
      {
        gsl_rng_set(random_generator, seedtable[i * Nmesh + j]);

        for (k = 0; k < Nmesh / 2; k++)
        {
          phase = gsl_rng_uniform(random_generator) * 2 * PI;
          // ***************** FAVN *****************
          phase += phase_shift;
          // ***************** FAVN *****************
          do
            ampl = gsl_rng_uniform(random_generator);

          while (ampl == 0);

          if (i == Nmesh / 2 || j == Nmesh / 2 || k == Nmesh / 2)
            continue;
          if (i == 0 && j == 0 && k == 0)
            continue;

          if (i < Nmesh / 2)
            kvec[0] = i * 2 * PI / Box;
          else
            kvec[0] = -(Nmesh - i) * 2 * PI / Box;

          if (j < Nmesh / 2)
            kvec[1] = j * 2 * PI / Box;
          else
            kvec[1] = -(Nmesh - j) * 2 * PI / Box;

          if (k < Nmesh / 2)
            kvec[2] = k * 2 * PI / Box;
          else
            kvec[2] = -(Nmesh - k) * 2 * PI / Box;

          kmag2 = kvec[0] * kvec[0] + kvec[1] * kvec[1] + kvec[2] * kvec[2];
          kmag = sqrt(kmag2);

          if (SphereMode == 1)
          {
            if (kmag * Box / (2 * PI) > Nsample / 2) /* select a sphere in k-space */
              continue;
          }
          else
          {
            if (fabs(kvec[0]) * Box / (2 * PI) > Nsample / 2)
              continue;
            if (fabs(kvec[1]) * Box / (2 * PI) > Nsample / 2)
              continue;
            if (fabs(kvec[2]) * Box / (2 * PI) > Nsample / 2)
              continue;
          }


          phig = Anorm * exp(PrimordialIndex * log(kmag)); /* initial normalized power */
                                                           // ************** FAVN/DSJ ***************
          if (!FixedAmplitude)
            phig *= -log(ampl);
          // ***************** FAVN/DSJ *************

          phig = sqrt(phig) * fac * Beta / kmag2; /* amplitude of the initial gaussian potential  (SG this converts phig to delta)*/

          if (k > 0)
          {
            if (i >= Local_x_start && i < (Local_x_start + Local_nx))
            {

              coord = ((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k;

              cpot[coord].re = phig * cos(phase);
              cpot[coord].im = phig * sin(phase);

              #ifdef OUTPUT_DF //SAM ADDED
                amplitudes[((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k] = phig;
                phases[((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k] = phase;
              #endif
            }
          }
          else /* k=0 plane needs special treatment */
          {
            if (i == 0)
            {
              if (j >= Nmesh / 2)
                continue;
              else
              {
                if (i >= Local_x_start && i < (Local_x_start + Local_nx))
                {
                  jj = Nmesh - j; /* note: j!=0 surely holds at this point */

                  coord = ((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k;

                  cpot[coord].re = phig * cos(phase);
                  cpot[coord].im = phig * sin(phase);

                  coord = ((i - Local_x_start) * Nmesh + jj) * (Nmesh / 2 + 1) + k;
                  cpot[coord].re = phig * cos(phase);
                  cpot[coord].im = -phig * sin(phase);

                  #ifdef OUTPUT_DF //SAM ADDED
                    amplitudes[((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k] = phig;
                    phases[((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k] = phase;
                    amplitudes[((i - Local_x_start) * Nmesh + jj) * (Nmesh / 2 + 1) + k] = phig;
    					      phases[((i - Local_x_start) * Nmesh + jj) * (Nmesh / 2 + 1) + k] = -phase;
                  #endif

                }
              }
            }
            else /* here comes i!=0 : conjugate can be on other processor! */
            {
              if (i >= Nmesh / 2)
                continue;
              else
              {
                ii = Nmesh - i;
                if (ii == Nmesh)
                  ii = 0;
                jj = Nmesh - j;
                if (jj == Nmesh)
                  jj = 0;

                if (i >= Local_x_start && i < (Local_x_start + Local_nx))
                {

                  coord = ((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k;

                  cpot[coord].re = phig * cos(phase);
                  cpot[coord].im = phig * sin(phase);

                  #ifdef OUTPUT_DF 
					          amplitudes[((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k] = phig;
					          phases[((i - Local_x_start) * Nmesh + j) * (Nmesh / 2 + 1) + k] = phase;
                  #endif
                }
                if (ii >= Local_x_start && ii < (Local_x_start + Local_nx))
                {
                  coord = ((ii - Local_x_start) * Nmesh + jj) * (Nmesh / 2 + 1) + k;

                  cpot[coord].re = phig * cos(phase);
                  cpot[coord].im = -phig * sin(phase);
                  #ifdef OUTPUT_DF
					          amplitudes[((ii - Local_x_start) * Nmesh + jj) * (Nmesh / 2 + 1) + k] = phig;
					          phases[((ii - Local_x_start) * Nmesh + jj) * (Nmesh / 2 + 1) + k] = -phase;
                  #endif					  
                }
              }
            }
          }
        }
      }
    }
  }


  // #ifdef OUTPUT_DF (DOES"T WORK FOR OSCILLATORY MODE. Only outputs P(k) so not super useful)
  // if (ThisTask == 0){
  //   printf("\nWriting linear field.");
  //   printf("Anorm = %.5e     Conversion = %.5e \n", Anorm,fac * Beta);
  // }
	//   write_density_field_data(); //NEED TO FIX THIS
  // #endif

  /*** For non-local models it is important to keep all factors of SQRT(-1) as done below ***/
  /*** Notice also that there is a minus to convert from Bardeen to gravitational potential ***/
  #ifdef LOCAL_FNL


    if (ThisTask == 0)
    {
      printf("Computing local non-Gaussian potential...");
      fflush(stdout);
    };

    /******* LOCAL PRIMORDIAL POTENTIAL ************/
    rfftwnd_mpi(Inverse_plan, 1, pot, Workspace, FFTW_NORMAL_ORDER);
    fflush(stdout);

    /* square the potential in configuration space */
    MPI_Barrier(MPI_COMM_WORLD); // Maybe not necessary?
    for (i = 0; i < Local_nx; i++)
      for (j = 0; j < Nmesh; j++)
        for (k = 0; k < Nmesh; k++)
        {
          coord = (i * Nmesh + j) * (2 * (Nmesh / 2 + 1)) + k;
          pot[coord] = pot[coord] + Fnl * pot[coord] * pot[coord];
        }

    /* Code to save the linear field */
    #ifdef OUTPUT_DF
      if (ThisTask == 0) printf("\nWriting linear field.");

      // Define global potential for I/O. Note that it is Nmesh*Nmesh*(Nmesh+2) because of zero padding!
      int local_size_pot = Local_nx * Nmesh * (2 * (Nmesh / 2 + 1)); 
      int nprocs = (Nmesh+1) / Local_nx;  //(Nmesh + Local_nx - 1) / Local_nx;                        // Number of processes
      pot_global = (fftw_real *)malloc(bytes = nprocs * sizeof(fftw_real) * TotalSizePlusAdditional);

      MPI_Barrier(MPI_COMM_WORLD);
      MPI_Allgather(pot, local_size_pot, MPI_DOUBLE, pot_global, local_size_pot, MPI_DOUBLE, MPI_COMM_WORLD);
      if(ThisTask == 0){
        // Open the file for writing
        FILE *file = fopen("output_potential.txt", "w");
        if (file == NULL) {
            fprintf(stderr, "Error: Could not open output_potential.txt for writing.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        for (i = 0; i < Nmesh; i++)
          for (j = 0; j < Nmesh; j++)
            for (k = 0; k < Nmesh; k++){
              coord = (i * Nmesh + j) * (2 * (Nmesh / 2 + 1)) + k;
              fprintf(file, "i= %d, j= %d, k= %d, pot= %.5e\n", i, j, k, pot_global[coord]); 
            }
        fclose(file);
      }
      free(pot_global);
    #endif

    
    MPI_Barrier(MPI_COMM_WORLD);
    rfftwnd_mpi(Forward_plan, 1, pot, Workspace, FFTW_NORMAL_ORDER);

    /* remove the N^3 I got by forwardfurier and put zero to zero mode */

    nmesh3 = ((unsigned int)Nmesh) * ((unsigned int)Nmesh) * ((unsigned int)Nmesh);
    for (i = 0; i < Local_nx; i++)
      for (j = 0; j < Nmesh; j++)
        for (k = 0; k <= Nmesh / 2; k++)
        {
          coord = (i * Nmesh + j) * (Nmesh / 2 + 1) + k;

          // ****************************** DSJ *************************
          if (SphereMode == 1)
          {
            ii = i + Local_x_start;

            if (ii < Nmesh / 2)
              kvec[0] = ii * 2 * PI / Box;
            else
              kvec[0] = -(Nmesh - ii) * 2 * PI / Box;

            if (j < Nmesh / 2)
              kvec[1] = j * 2 * PI / Box;
            else
              kvec[1] = -(Nmesh - j) * 2 * PI / Box;

            if (k < Nmesh / 2)
              kvec[2] = k * 2 * PI / Box;
            else
              kvec[2] = -(Nmesh - k) * 2 * PI / Box;

            kmag = sqrt(kvec[0] * kvec[0] + kvec[1] * kvec[1] + kvec[2] * kvec[2]);

            if (kmag * Box / (2 * PI) > Nsample / 2)
            { /* select a sphere in k-space */
              cpot[coord].re = 0.;
              cpot[coord].im = 0.;
              continue;
            }
          }
          // ****************************** DSJ *************************

          cpot[coord].re /= (double)nmesh3;
          cpot[coord].im /= (double)nmesh3;
        }

    if (ThisTask == 0)
    {
      cpot[0].re = 0.;
      cpot[0].im = 0.;
    }

    if (ThisTask == 0)
      print_timed_done(7);

  #else

  #ifdef QSFI_FNL
  // ********************** Collider Addition (Start) ****************************
    if (ThisTask == 0)
    {
      printf("\nComputing QSFI non-Gaussian potential... ");
      fflush(stdout);
    };

    // Initialize FFT's for auxiliary field
    ckdeltaphi = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
    kdeltaphi = (fftw_real *)ckdeltaphi;
    ASSERT_ALLOC(ckdeltaphi);

    cpsi = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
    psi = (fftw_real *)cpsi;
    ASSERT_ALLOC(cpsi);

    cpot_sq = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
    pot_sq = (fftw_real *)cpot_sq;
    ASSERT_ALLOC(cpot_sq);

    // Construct psi field
    MPI_Barrier(MPI_COMM_WORLD);
    // Clean all arrays
    for (i = 0; i < Local_nx; i++)
      for (j = 0; j < Nmesh; j++)
        for (k = 0; k <= Nmesh / 2; k++)
        {
          coord = (i * Nmesh + j) * (Nmesh / 2 + 1) + k;
          ckdeltaphi[coord].re = 0.0;
          ckdeltaphi[coord].im = 0.0;
          cpsi[coord].re = 0.0;
          cpsi[coord].im = 0.0;
          cpot_sq[coord].re = 0.0;
          cpot_sq[coord].im = 0.0;
        }

    
    // Multiply by k^Delta
    MPI_Barrier(MPI_COMM_WORLD);
    for (ii = 0; ii < Local_nx; ii++)
      for (j = 0; j < Nmesh; j++)
        for (k = 0; k <= Nmesh / 2; k++)
        {
          coord = (ii * Nmesh + j) * (Nmesh / 2 + 1) + k;
          i = ii + Local_x_start;

          // Get knorm
          if (i < Nmesh / 2)
            kvec[0] = i * 2 * PI / Box;
          else
            kvec[0] = -(Nmesh - i) * 2 * PI / Box;

          if (j < Nmesh / 2)
            kvec[1] = j * 2 * PI / Box;
          else
            kvec[1] = -(Nmesh - j) * 2 * PI / Box;

          if (k < Nmesh / 2)
            kvec[2] = k * 2 * PI / Box;
          else
            kvec[2] = -(Nmesh - k) * 2 * PI / Box;

          kmag = sqrt(kvec[0] * kvec[0] + kvec[1] * kvec[1] + kvec[2] * kvec[2]);
          kmag_Delta_QSFI = pow(kmag, Delta/3.*(4.-PrimordialIndex)); 
          ckdeltaphi[coord].re = kmag_Delta_QSFI * cpot[coord].re;
          ckdeltaphi[coord].im = kmag_Delta_QSFI * cpot[coord].im;

        }

    // Fourier transform back to real space
    MPI_Barrier(MPI_COMM_WORLD);
    rfftwnd_mpi(Inverse_plan, 1, pot, Workspace, FFTW_NORMAL_ORDER);
    rfftwnd_mpi(Inverse_plan, 1, kdeltaphi, Workspace, FFTW_NORMAL_ORDER);

    // Construct psi field
    MPI_Barrier(MPI_COMM_WORLD);
    /* Compute real space product for psi */
    for (i = 0; i < Local_nx; i++)
      for (j = 0; j < Nmesh; j++)
        for (k = 0; k < Nmesh; k++)
        {
          coord = (i * Nmesh + j) * (2 * (Nmesh / 2 + 1)) + k;
          /* Following line computes psi(x)=phi_g(x)*F(k^DeltaPhi_g)[x]
              To get the full psi(x) we need to do the following:
              1) Go back to fourier space and comptue 2/k^Delta*psi(k)
              2) Go back to real space and subtract off Phi^2(x)
          */
          psi[coord] = kdeltaphi[coord] * pot[coord];
          pot_sq[coord] = pot[coord]*pot[coord]; 
        }

    // Fourier transform back to Fourier space
    MPI_Barrier(MPI_COMM_WORLD);
    rfftwnd_mpi(Forward_plan, 1, psi, Workspace, FFTW_NORMAL_ORDER);
    rfftwnd_mpi(Forward_plan, 1, pot_sq, Workspace, FFTW_NORMAL_ORDER);

    // Multiply by 2/k^Delta
    MPI_Barrier(MPI_COMM_WORLD);
    nmesh3 = ((unsigned int)Nmesh) * ((unsigned int)Nmesh) * ((unsigned int)Nmesh);
    for (ii = 0; ii < Local_nx; ii++)
      for (j = 0; j < Nmesh; j++)
        for (k = 0; k <= Nmesh / 2; k++)
        {
          coord = (ii * Nmesh + j) * (Nmesh / 2 + 1) + k;
          i = ii + Local_x_start;

          // Get knorm
          if (i < Nmesh / 2)
            kvec[0] = i * 2 * PI / Box;
          else
            kvec[0] = -(Nmesh - i) * 2 * PI / Box;

          if (j < Nmesh / 2)
            kvec[1] = j * 2 * PI / Box;
          else
            kvec[1] = -(Nmesh - j) * 2 * PI / Box;

          if (k < Nmesh / 2)
            kvec[2] = k * 2 * PI / Box;
          else
            kvec[2] = -(Nmesh - k) * 2 * PI / Box;

          kmag = sqrt(kvec[0] * kvec[0] + kvec[1] * kvec[1] + kvec[2] * kvec[2]);
          kmag_Delta_QSFI = pow(kmag, Delta/3.*(4.-PrimordialIndex));

          // Set minimum |k| for numerical stability. 1e-12 should be sufficiently
          // small since this corresponds to kF for a ____ Gpc box (in units of h/kpc)
          if (kmag_Delta_QSFI < 1e-12){
            kmag_Delta_QSFI = 1e-12;
          }
          cpsi[coord].re = 2. / kmag_Delta_QSFI * cpsi[coord].re - cpot_sq[coord].re;
          cpsi[coord].im = 2. / kmag_Delta_QSFI * cpsi[coord].im - cpot_sq[coord].im;

          // Apply high-pass filter
          // if((kmag * 1000) < Klong_max){
          //   cpsi[coord].re = 0.;
          //   cpsi[coord].im = 0.;
          // }

          cpsi[coord].re *= 0.5*(1 + tanh((kmag*1000 - 0.08) /(0.01) - 1 ));
          cpsi[coord].im *= 0.5*(1 + tanh((kmag*1000 - 0.08) /(0.01) - 1 ));

          // Normalize from FFT and set zero mode to zero
          cpsi[coord].re /= (double) nmesh3; 
          cpsi[coord].im /= (double) nmesh3;
          if(i == 0 && j == 0 && k == 0){
            cpsi[0].re=0.;
            cpsi[0].im=0.;
            continue;
          }

        }

    // Go back to real space and add psi to phi
    MPI_Barrier(MPI_COMM_WORLD);
    rfftwnd_mpi(Inverse_plan, 1, psi, Workspace, FFTW_NORMAL_ORDER);
    
    MPI_Barrier(MPI_COMM_WORLD);
    for (i = 0; i < Local_nx; i++)
      for (j = 0; j < Nmesh; j++)
        for (k = 0; k < Nmesh; k++){
          coord = (i * Nmesh + j) * (2 * (Nmesh / 2 + 1)) + k; 
          pot[coord] = pot[coord] + Fnl * (psi[coord]); 

    }
    
      /* Code to save the linear field */
    #ifdef OUTPUT_DF
      if (ThisTask == 0) printf("\nWriting linear field.");

      // Define global potential for I/O. Note that it is Nmesh*Nmesh*(Nmesh+2) because of zero padding!
      int local_size_pot = Local_nx * Nmesh * (2 * (Nmesh / 2 + 1)); 
      int nprocs = (Nmesh+1) / Local_nx;
      pot_global = (fftw_real *)malloc(bytes = nprocs * sizeof(fftw_real) * TotalSizePlusAdditional);

      MPI_Barrier(MPI_COMM_WORLD);
      MPI_Allgather(pot, local_size_pot, MPI_DOUBLE, pot_global, local_size_pot, MPI_DOUBLE, MPI_COMM_WORLD);
      if(ThisTask == 0){
        // Open the file for writing
        FILE *file = fopen("output_potential.txt", "w");
        if (file == NULL) {
            fprintf(stderr, "Error: Could not open output_potential.txt for writing.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        for (i = 0; i < Nmesh; i++)
          for (j = 0; j < Nmesh; j++)
            for (k = 0; k < Nmesh; k++){
              coord = (i * Nmesh + j) * (2 * (Nmesh / 2 + 1)) + k;
              fprintf(file, "i= %d, j= %d, k= %d, pot= %.5e\n", i, j, k, pot_global[coord]); 
            }
        fclose(file);
      }
      free(pot_global);
    #endif

    
    // Go back to Fourier space, remove 1/N^3 factor, and set zero mode to zero
    MPI_Barrier(MPI_COMM_WORLD);
    rfftwnd_mpi(Forward_plan, 1, pot, Workspace, FFTW_NORMAL_ORDER);

    /* remove the N^3 I got by forwardfurier and put zero to zero mode */

    for (i = 0; i < Local_nx; i++)
      for (j = 0; j < Nmesh; j++)
        for (k = 0; k <= Nmesh / 2; k++)
        {
          coord = (i * Nmesh + j) * (Nmesh / 2 + 1) + k;
          // ****************************** DSJ *************************
          if (SphereMode == 1)
          {
            ii = i + Local_x_start;

            if (ii < Nmesh / 2)
              kvec[0] = ii * 2 * PI / Box;
            else
              kvec[0] = -(Nmesh - ii) * 2 * PI / Box;

            if (j < Nmesh / 2)
              kvec[1] = j * 2 * PI / Box;
            else
              kvec[1] = -(Nmesh - j) * 2 * PI / Box;

            if (k < Nmesh / 2)
              kvec[2] = k * 2 * PI / Box;
            else
              kvec[2] = -(Nmesh - k) * 2 * PI / Box;

            kmag = sqrt(kvec[0] * kvec[0] + kvec[1] * kvec[1] + kvec[2] * kvec[2]);

            if (kmag * Box / (2 * PI) > Nsample / 2)
            { /* select a sphere in k-space */
              cpot[coord].re = 0.;
              cpot[coord].im = 0.;
              continue;
            }
          }
          // ****************************** DSJ *************************
          cpot[coord].re /= (double)nmesh3;
          cpot[coord].im /= (double)nmesh3;
        }

    if (ThisTask == 0)
    {
      cpot[0].re = 0.;
      cpot[0].im = 0.;
    }

    if (ThisTask == 0 ) print_timed_done(1);
    free(cpsi);
    free(ckdeltaphi);
#else
#ifdef OSC_FNL
  if (ThisTask == 0)
    {
      printf("Computing oscillatory bispectra non-Gaussian potential...\n ");
      fflush(stdout);
    };



     // We never FFT the full fields. We just use it to store the full k-space 
     // field before splitting into real and imaginary parts
    ck_Delta_plus_inu_phi_full = (fftw_complex *)malloc(bytes = sizeof(fftw_complex) * TotalSizePlusAdditional);
    ck_Delta_min_inu_phi_full = (fftw_complex *)malloc(bytes = sizeof(fftw_complex) * TotalSizePlusAdditional);
    ASSERT_ALLOC(ck_Delta_plus_inu_phi_full);
    ASSERT_ALLOC(ck_Delta_min_inu_phi_full);
    
    // Real and imaginary components of full FFT needed for intermediate steps

    // Plus field
    ck_Delta_plus_inu_phi_real = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
    k_Delta_plus_inu_phi_real = (fftw_real *)ck_Delta_plus_inu_phi_real;
    ASSERT_ALLOC(ck_Delta_plus_inu_phi_real);

    ck_Delta_plus_inu_phi_imag = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
    k_Delta_plus_inu_phi_imag = (fftw_real *)ck_Delta_plus_inu_phi_imag;
    ASSERT_ALLOC(ck_Delta_plus_inu_phi_imag);

    // Minus field
    ck_Delta_min_inu_phi_real = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
    k_Delta_min_inu_phi_real = (fftw_real *)ck_Delta_min_inu_phi_real;
    ASSERT_ALLOC(ck_Delta_min_inu_phi_real);

    ck_Delta_min_inu_phi_imag = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
    k_Delta_min_inu_phi_imag = (fftw_real *)ck_Delta_min_inu_phi_imag;
    ASSERT_ALLOC(ck_Delta_min_inu_phi_imag);

    cpsi = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
    psi = (fftw_real *)cpsi;
    ASSERT_ALLOC(cpsi);

    cpot_sq = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
    pot_sq = (fftw_real *)cpot_sq;
    ASSERT_ALLOC(cpot_sq);

    int local_size = Local_nx * Nmesh * (Nmesh / 2 + 1); // Size of local `cpot` chunk
    int nprocs = (Nmesh+1) / Local_nx;  

        
    cpot_received = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
    ck_Delta_plus_inu_phi_full_received= (fftw_complex *)malloc(bytes =  sizeof(fftw_complex) * TotalSizePlusAdditional);
    ck_Delta_min_inu_phi_full_received = (fftw_complex *)malloc(bytes =  sizeof(fftw_complex) * TotalSizePlusAdditional);
    ASSERT_ALLOC(cpot_received);
    ASSERT_ALLOC(ck_Delta_plus_inu_phi_full_received);
    ASSERT_ALLOC(ck_Delta_min_inu_phi_full_received);

    // Clean all arrays
    for (i = 0; i < Local_nx; i++)
      for (j = 0; j < Nmesh; j++)
        for (k=0; k<Nmesh; k++){   //for (k = 0; k <= Nmesh / 2; k++)

          coord = (i * Nmesh + j) * Nmesh + k;

          ck_Delta_plus_inu_phi_full[coord].re = 0.0;
          ck_Delta_plus_inu_phi_full[coord].im = 0.0;
          ck_Delta_min_inu_phi_full[coord].re = 0.0;
          ck_Delta_min_inu_phi_full[coord].im = 0.0;

          // Fill entires for real FFT
          if(k<= Nmesh/2){
            coord = (i * Nmesh + j) * (Nmesh / 2 + 1) + k;
            ck_Delta_plus_inu_phi_real[coord].re = 0.0;
            ck_Delta_plus_inu_phi_real[coord].im = 0.0;
            ck_Delta_plus_inu_phi_imag[coord].re = 0.0;
            ck_Delta_plus_inu_phi_imag[coord].im = 0.0;

            ck_Delta_min_inu_phi_real[coord].re = 0.0;
            ck_Delta_min_inu_phi_real[coord].im = 0.0;
            ck_Delta_min_inu_phi_imag[coord].re = 0.0;
            ck_Delta_min_inu_phi_imag[coord].im = 0.0;

            cpsi[coord].re = 0.0;
            cpsi[coord].im = 0.0;

            cpot_sq[coord].re = 0.0;
            cpot_sq[coord].im = 0.0;
            }
          }


    //MPI_Barrier(MPI_COMM_WORLD);

    int partner = nprocs - 1 - ThisTask;
    MPI_Sendrecv(cpot, local_size, MPI_C_DOUBLE_COMPLEX, partner, 0,
                 cpot_received, local_size, MPI_C_DOUBLE_COMPLEX, partner, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
           
    printf("Task %d received cpot from task %d\n", ThisTask, partner);
    if (ThisTask == 0) printf("-> Gathered cpot from all processes\n");

    // Construct full FFT for k^(Delta+/-i*nu)phi(k) 
    for (ii = 0; ii < Local_nx; ii++) {
      for (j = 0; j < Nmesh; j++) {
        for (k = 0; k < Nmesh; k++) { // Full k range
          coord = (ii * Nmesh + j) * Nmesh + k; // 3D flattened index
          i = ii + Local_x_start;

          // Get kvec components
          if (i < Nmesh / 2)
              kvec[0] = i * 2 * PI / Box;
          else
              kvec[0] = -(Nmesh - i) * 2 * PI / Box;

          if (j < Nmesh / 2)
              kvec[1] = j * 2 * PI / Box;
          else
              kvec[1] = -(Nmesh - j) * 2 * M_PI / Box;

          if (k < Nmesh / 2)
              kvec[2] = k * 2 * PI / Box;
          else
              kvec[2] = -(Nmesh - k) * 2 * PI / Box;

          // Compute magnitude of k
          kmag = sqrt(kvec[0] * kvec[0] + kvec[1] * kvec[1] + kvec[2] * kvec[2]);

          // Compute k^{3/2} +/- iNu

          /* OLD CODE
          kmag_Delta_plus_inu = cpow(kmag, Delta + I * Nu);
          kmag_Delta_min_inu  = cpow(kmag, Delta - I * Nu);
          */

         kmag_Delta_plus_inu = cexp(log(kmag) * (Delta + I * Nu)) + 1.e-20;  // Regularize
         kmag_Delta_min_inu  = cexp(log(kmag) * (Delta - I * Nu)) + 1.e-20;

          // Store in 3D array
          if (i == 0 && j == 0 && k == 0){
            ck_Delta_plus_inu_phi_full[coord].re = 0.0;
            ck_Delta_plus_inu_phi_full[coord].im = 0.0;
            ck_Delta_min_inu_phi_full[coord].re = 0.0;
            ck_Delta_min_inu_phi_full[coord].im = 0.0;
          }
          else{

            /*
            Multiply by phi(k). If k<=Nmesh/2, we can just use the local cpot 
            array and apply local indexing (i.e use ii). For k>Nmesh/2, things
            are more subtle because we need to access the Hermitian conjugate
            from the global_cpot array. We are explicitly using that phi(-k)=
            phi*(k).
            */
            if(k<= Nmesh/2){
              coord_1d = (ii * Nmesh + j) * (Nmesh / 2 + 1) + k; // Use local index here because we don't need global array

              temp_complex =  (kmag_Delta_plus_inu)*(cpot[coord_1d].re+I*cpot[coord_1d].im);
              ck_Delta_plus_inu_phi_full[coord].re = creal(temp_complex);
              ck_Delta_plus_inu_phi_full[coord].im = cimag(temp_complex);

              temp_complex =  (kmag_Delta_min_inu)*(cpot[coord_1d].re+I*cpot[coord_1d].im);
              ck_Delta_min_inu_phi_full[coord].re  = creal(temp_complex);
              ck_Delta_min_inu_phi_full[coord].im  = cimag(temp_complex);

            }else{

              // Use fact that phi(-k)=phi*(k) to get phi(k) for k>Nmesh/2
              i_herm = (Nmesh-i) % Nmesh; // Note we use i instead of ii here bceause we need global index!
              j_herm = (Nmesh-j) % Nmesh;
              k_herm = (Nmesh-k) % Nmesh;

              ///* Send and receive code
              i_herm = i_herm%Local_nx; // (or possibly (Local_nx-i_herm%Local_nx)%Local_nx)
              coord_1d = (i_herm * Nmesh + j_herm) * (Nmesh / 2 + 1) + k_herm;

              /* OLD CODE
              if(i_herm==0){
                // For zero mode we use the local potential array.
                temp_complex = kmag_Delta_plus_inu*(cpot[coord_1d].re+I*cpot[coord_1d].im);       
                ck_Delta_plus_inu_phi_full[coord].re = creal(temp_complex);
                ck_Delta_plus_inu_phi_full[coord].im = -cimag(temp_complex); // Note the minus for complex conjugate

                temp_complex =  kmag_Delta_min_inu*(cpot[coord_1d].re+I*cpot[coord_1d].im);
                ck_Delta_min_inu_phi_full[coord].re  = creal(temp_complex);
                ck_Delta_min_inu_phi_full[coord].im  = -cimag(temp_complex); // Note the minus for complex conjugate 
              } else{

                temp_complex = kmag_Delta_plus_inu*(cpot_received[coord_1d].re+I*cpot_received[coord_1d].im);       
                ck_Delta_plus_inu_phi_full[coord].re = creal(temp_complex);
                ck_Delta_plus_inu_phi_full[coord].im = -cimag(temp_complex); // Note the minus for complex conjugate

                temp_complex =  kmag_Delta_min_inu*(cpot_received[coord_1d].re+I*cpot_received[coord_1d].im);
                ck_Delta_min_inu_phi_full[coord].re  = creal(temp_complex);
                ck_Delta_min_inu_phi_full[coord].im  = -cimag(temp_complex); // Note the minus for complex conjugate 
              }*/

              // NEW CODE. TRY ONLY TAKING CC OF CPOT INSTEAD OF FULL ARRAY
              if(i_herm==0){
                // For zero mode we use the local potential array.
                temp_complex = kmag_Delta_plus_inu*(cpot[coord_1d].re-I*cpot[coord_1d].im);       
                ck_Delta_plus_inu_phi_full[coord].re = creal(temp_complex);
                ck_Delta_plus_inu_phi_full[coord].im = cimag(temp_complex); 

                temp_complex =  kmag_Delta_min_inu*(cpot[coord_1d].re-I*cpot[coord_1d].im);
                ck_Delta_min_inu_phi_full[coord].re  = creal(temp_complex);
                ck_Delta_min_inu_phi_full[coord].im  = cimag(temp_complex); 
              } else{

                temp_complex = kmag_Delta_plus_inu*(cpot_received[coord_1d].re-I*cpot_received[coord_1d].im);       
                ck_Delta_plus_inu_phi_full[coord].re = creal(temp_complex);
                ck_Delta_plus_inu_phi_full[coord].im = cimag(temp_complex); // Note the minus for complex conjugate

                temp_complex =  kmag_Delta_min_inu*(cpot_received[coord_1d].re-I*cpot_received[coord_1d].im);
                ck_Delta_min_inu_phi_full[coord].re  = creal(temp_complex);
                ck_Delta_min_inu_phi_full[coord].im  = cimag(temp_complex); // Note the minus for complex conjugate 
              }
            }
          }
        }
      }
    }

    // Convert full FFT the outputs of two real FFTs. The following code takes
    // an NMesh^3 dimensional complex FFT (in Fourier) space, called Input,
    // and splits it into two Nmesh*Nmesh*(NMesh//2+1) dimensional real FFTs 
    // (in Fourier space), corresponding to FFT[Re[IFFT[Input]]] and FFT[Im[IFFT[Input]]]

    // Gather `ck_Delta_plus_inu_phi_full` and `ck_Delta_min_inu_phi_full` from all processes
    if (ThisTask == 0){
      printf("-> Freeing global potential and gathering global phi^Delta_+/- arrays...\n");
      fflush(stdout);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    free(cpot_received);

    local_size = Local_nx * Nmesh * Nmesh; // Size for complex FFT
    MPI_Sendrecv(ck_Delta_plus_inu_phi_full, local_size, MPI_C_DOUBLE_COMPLEX, partner, 0,
                 ck_Delta_plus_inu_phi_full_received, local_size, MPI_C_DOUBLE_COMPLEX, partner, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Sendrecv(ck_Delta_min_inu_phi_full, local_size, MPI_C_DOUBLE_COMPLEX, partner, 0,
                 ck_Delta_min_inu_phi_full_received, local_size, MPI_C_DOUBLE_COMPLEX, partner, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);


    
    if (ThisTask == 0){
      printf("-> Splitting full FFT into real FFTs...\n");
      fflush(stdout);
    }
    for (ii = 0; ii < Local_nx; ii++) {
      for (j = 0; j < Nmesh; j++) {
        for (k = 0; k <= Nmesh / 2; k++) {
          /*
          Some notes:
            * Need to use the full index for the ck_Delta_plus_inu_full array
            * Will need a different index when assigning ck_Delta_plus_inu_phi_real
            * i = ii + Local_x_start; Don't need this for now. ii is the relevant one for indexing?
            * Parallelization could be an issue here because the indices are split over multiple threads.
            * Understand why cimag(temp_real) and cimag(temp_imag) are zero.
          */
          
          // Coord_1d is for assignemnt to ck_Delta_+/-_inu_phi_real and ck_Delta_+/-_inu_phi_imag
          coord_1d = (ii * Nmesh + j) * (Nmesh / 2 + 1) + k;

          // Get 1D index for hermitian conjugate
          i = ii + Local_x_start;
          i_herm = (Nmesh-i) % Nmesh;
          j_herm = (Nmesh-j) % Nmesh;
          k_herm = (Nmesh-k) % Nmesh;

          coord = (ii * Nmesh + j) * Nmesh+k; // Use local index when accessing ck_..._full
          

          
          /*
          * Split k^(Delta+iNu)phi(k)
          */
          temp_ck_coord = ck_Delta_plus_inu_phi_full[coord].re+I*ck_Delta_plus_inu_phi_full[coord].im;

          i_herm = i_herm%Local_nx; 
          coord_herm = (i_herm * Nmesh + j_herm) * Nmesh + k_herm;

          if(i_herm==0){
            temp_ck_herm  = ck_Delta_plus_inu_phi_full[coord_herm].re-I*ck_Delta_plus_inu_phi_full[coord_herm].im;
          } else{
            temp_ck_herm  = ck_Delta_plus_inu_phi_full_received[coord_herm].re-I*ck_Delta_plus_inu_phi_full_received[coord_herm].im; // Conjugate transpose
          }

          temp_real = 0.5*(temp_ck_coord+temp_ck_herm);
          temp_imag = -0.5*I*(temp_ck_coord-temp_ck_herm);

          // Compute RFFT index and assign values
          ck_Delta_plus_inu_phi_real[coord_1d].re = creal(temp_real);
          ck_Delta_plus_inu_phi_real[coord_1d].im = cimag(temp_real);
          ck_Delta_plus_inu_phi_imag[coord_1d].re = creal(temp_imag);
          ck_Delta_plus_inu_phi_imag[coord_1d].im = cimag(temp_imag); 

          /*
          * Split k^(Delta-iNu)phi(k)
          */ 

          temp_ck_coord = ck_Delta_min_inu_phi_full[coord].re+I*ck_Delta_min_inu_phi_full[coord].im;

          if(i_herm==0){
            // For zero mode we use the local ck_Delta_plus_inu_phi_full array.
            temp_ck_herm  = ck_Delta_min_inu_phi_full[coord_herm].re-I*ck_Delta_min_inu_phi_full[coord_herm].im; // Conjugate transpose
          } else{
            temp_ck_herm  = ck_Delta_min_inu_phi_full_received[coord_herm].re-I*ck_Delta_min_inu_phi_full_received[coord_herm].im; // Conjugate transpose
          }

          temp_real = 0.5*(temp_ck_coord+temp_ck_herm);
          temp_imag = -0.5*I*(temp_ck_coord-temp_ck_herm);

          // Compute RFFT index and assign values
          ck_Delta_min_inu_phi_real[coord_1d].re = creal(temp_real);
          ck_Delta_min_inu_phi_real[coord_1d].im = cimag(temp_real);
          ck_Delta_min_inu_phi_imag[coord_1d].re = creal(temp_imag);
          ck_Delta_min_inu_phi_imag[coord_1d].im = cimag(temp_imag); 
        }
      }
    }

    if (ThisTask == 0){
      printf("-> Freeing global phi^Delta_+/- arrays and computing FFTS...\n");
      fflush(stdout);
    }

    // Go back to real space
    MPI_Barrier(MPI_COMM_WORLD);

    if (ThisTask == 0){
      printf("-> Local freed...\n");
      fflush(stdout);
    }

    free(ck_Delta_plus_inu_phi_full);
    ck_Delta_plus_inu_phi_full = NULL;

    free(ck_Delta_min_inu_phi_full);
    ck_Delta_min_inu_phi_full = NULL; 

    free(ck_Delta_plus_inu_phi_full_received);
    ck_Delta_plus_inu_phi_full_received = NULL;

    free(ck_Delta_min_inu_phi_full_received);
    ck_Delta_min_inu_phi_full_received = NULL;

    
    if (ThisTask == 0){
      printf("-> Global freed...\n");
      fflush(stdout);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    rfftwnd_mpi(Inverse_plan, 1, pot, Workspace, FFTW_NORMAL_ORDER);
    rfftwnd_mpi(Inverse_plan, 1, k_Delta_plus_inu_phi_real, Workspace, FFTW_NORMAL_ORDER);
    rfftwnd_mpi(Inverse_plan, 1, k_Delta_plus_inu_phi_imag, Workspace, FFTW_NORMAL_ORDER);
    rfftwnd_mpi(Inverse_plan, 1, k_Delta_min_inu_phi_real, Workspace, FFTW_NORMAL_ORDER);
    rfftwnd_mpi(Inverse_plan, 1, k_Delta_min_inu_phi_imag, Workspace, FFTW_NORMAL_ORDER);


    if (ThisTask == 0){
      printf("-> Constructing Phi(x) field..\n");
      fflush(stdout);
    }


    // Multiply fields by phi(x) in real space. Should probably introduce a 
    // different variable name here, but saving memory by just replacing entries
    MPI_Barrier(MPI_COMM_WORLD);

    /* Compute real space product for psi */
    for (i = 0; i < Local_nx; i++)
      for (j = 0; j < Nmesh; j++)
        for (k = 0; k < Nmesh; k++)
        {
          coord = (i * Nmesh + j) * (2 * (Nmesh / 2 + 1)) + k;
          /* 
          Following line computes psi(x)=phi_g(x)*F(k^(Delta+/-inu)Phi_g)[x]
          */
  
          k_Delta_plus_inu_phi_real[coord] *= pot[coord];
          k_Delta_plus_inu_phi_imag[coord] *= pot[coord];
          k_Delta_min_inu_phi_real[coord]  *= pot[coord];
          k_Delta_min_inu_phi_imag[coord]  *= pot[coord];
          pot_sq[coord] = pot[coord]*pot[coord]; 
        }

    if (ThisTask == 0){
      printf("-> Going back to Fourier space..\n");
      fflush(stdout);
    }

    // Fourier transform back to Fourier space
    MPI_Barrier(MPI_COMM_WORLD);
    rfftwnd_mpi(Forward_plan, 1, k_Delta_plus_inu_phi_real, Workspace, FFTW_NORMAL_ORDER);
    rfftwnd_mpi(Forward_plan, 1, k_Delta_plus_inu_phi_imag, Workspace, FFTW_NORMAL_ORDER);
    rfftwnd_mpi(Forward_plan, 1, k_Delta_min_inu_phi_real, Workspace, FFTW_NORMAL_ORDER);
    rfftwnd_mpi(Forward_plan, 1, k_Delta_min_inu_phi_imag, Workspace, FFTW_NORMAL_ORDER);
    rfftwnd_mpi(Forward_plan, 1, pot_sq, Workspace, FFTW_NORMAL_ORDER);

    if (ThisTask == 0){
      printf("-> Constructing Psi(x) field..\n");
      fflush(stdout);
    }


    /*
    Construct psi fields
    */
    nmesh3 = ((unsigned int)Nmesh) * ((unsigned int)Nmesh) * ((unsigned int)Nmesh);
    for (ii = 0; ii < Local_nx; ii++)
      for (j = 0; j < Nmesh; j++)
        for (k = 0; k <= Nmesh / 2; k++)
        {
          coord = (ii * Nmesh + j) * (Nmesh / 2 + 1) + k;
          i = ii + Local_x_start;

          // Get knorm
          if (i < Nmesh / 2)
            kvec[0] = i * 2 * PI / Box;
          else
            kvec[0] = -(Nmesh - i) * 2 * PI / Box;

          if (j < Nmesh / 2)
            kvec[1] = j * 2 * PI / Box;
          else
            kvec[1] = -(Nmesh - j) * 2 * PI / Box;

          if (k < Nmesh / 2)
            kvec[2] = k * 2 * PI / Box;
          else
            kvec[2] = -(Nmesh - k) * 2 * PI / Box;

          kmag = sqrt(kvec[0] * kvec[0] + kvec[1] * kvec[1] + kvec[2] * kvec[2]);

          // Compute k^{3/2} +/- iNu
          /* OLD CODE
          kmag_Delta_plus_inu = cpow(kmag, Delta + I * Nu);
          kmag_Delta_min_inu  = cpow(kmag, Delta - I * Nu);
          */

         kmag_Delta_plus_inu = cexp(log(kmag) * (Delta + I * Nu)) + 1.e-20;  // Regularize
         kmag_Delta_min_inu  = cexp(log(kmag) * (Delta - I * Nu)) + 1.e-20;


          // Compute exp(+\- phase)
          exp_plus_iphase = cexp(I * Phase);
          exp_min_iphase = cexp(-I * Phase);
          
          // Check this. It is just supposed to be the sum of the two FFTs (accounting for I) divided by k^plus
          temp_cpsi_plus = (ck_Delta_plus_inu_phi_real[coord].re+I*ck_Delta_plus_inu_phi_real[coord].im+
                                           I * (ck_Delta_plus_inu_phi_imag[coord].re+I*ck_Delta_plus_inu_phi_imag[coord].im))/kmag_Delta_plus_inu;

          temp_cpsi_min = (ck_Delta_min_inu_phi_real[coord].re+I*ck_Delta_min_inu_phi_real[coord].im+
                                           I * (ck_Delta_min_inu_phi_imag[coord].re+I*ck_Delta_min_inu_phi_imag[coord].im))/kmag_Delta_min_inu;
          
          // Subtract off 0.5 phi^2(x) and multiply by phase
          temp_cpsi_plus -= 0.5*(cpot_sq[coord].re+I*cpot_sq[coord].im);
          temp_cpsi_min  -= 0.5*(cpot_sq[coord].re+I*cpot_sq[coord].im);
          temp_cpsi_plus *= exp_plus_iphase;
          temp_cpsi_min  *= exp_min_iphase;

          // Set final psi array
          cpsi[coord].re = creal(temp_cpsi_plus)+creal(temp_cpsi_min);
          cpsi[coord].im = cimag(temp_cpsi_plus)+cimag(temp_cpsi_min);


          // // Set minimum |k| for numerical stability. 1e-12 should be sufficiently
          // // small since this corresponds to kF for a ____ Gpc box (in units of h/kpc)
          // if (kmag_Delta_QSFI < 1e-12){
          //   kmag_Delta_QSFI = 1e-12;
          // }

          // // Apply high-pass filter (in h/Mpc)
          // if((kmag * 1000) < Klong_max){
          //   // Print filtering
          //   // if (ThisTask == 0){
          //   //   printf("Filtering k=(%d, %d, %d) with kmag=%.2e\n", i, j, k, kmag*1000);
          //   //   fflush(stdout);  
          //   // }
          //   cpsi[coord].re = 0.;
          //   cpsi[coord].im = 0.;
          // }


          cpsi[coord].re *= 0.5*(1 + tanh((kmag*1000 - 0.08) /(0.01) - 1 ));
          cpsi[coord].im *= 0.5*(1 + tanh((kmag*1000 - 0.08) /(0.01) - 1 ));

          // Normalize from FFT and set zero mode to zero
          cpsi[coord].re /= (double) nmesh3; 
          cpsi[coord].im /= (double) nmesh3;
          if(i == 0 && j == 0 && k == 0){
            cpsi[0].re=0.;
            cpsi[0].im=0.;
            continue;
          }

        }

    if (ThisTask == 0){
      printf("-> Constructing Phi_NG(x)=Phi_g(x)+fNL*Psi(x)..\n");
      fflush(stdout);
    }

    // Go back to real space and add psi to phi
    MPI_Barrier(MPI_COMM_WORLD);
    rfftwnd_mpi(Inverse_plan, 1, psi, Workspace, FFTW_NORMAL_ORDER);
    
    for (i = 0; i < Local_nx; i++)
      for (j = 0; j < Nmesh; j++)
        for (k = 0; k < Nmesh; k++){
          coord = (i * Nmesh + j) * (2 * (Nmesh / 2 + 1)) + k; 
          pot[coord] = pot[coord] + Fnl * (psi[coord]); 

    }

    if (ThisTask == 0){
      printf("-> Freeing arrays..\n");
      fflush(stdout);
    }

    free(ck_Delta_plus_inu_phi_real);
    free(ck_Delta_plus_inu_phi_imag);
    free(ck_Delta_min_inu_phi_real);
    free(ck_Delta_min_inu_phi_imag);
    

    /* Code to save the linear field */
    #ifdef OUTPUT_DF
      if (ThisTask == 0) printf("\nWriting linear field.");

      // Define global potential for I/O. Note that it is Nmesh*Nmesh*(Nmesh+2) because of zero padding!
      int local_size_pot = Local_nx * Nmesh * (2 * (Nmesh / 2 + 1)); 
      pot_global = (fftw_real *)malloc(bytes = nprocs * sizeof(fftw_real) * TotalSizePlusAdditional);

      MPI_Barrier(MPI_COMM_WORLD);
      MPI_Allgather(pot, local_size_pot, MPI_DOUBLE, pot_global, local_size_pot, MPI_DOUBLE, MPI_COMM_WORLD);
      if(ThisTask == 0){
        // Open the file for writing
        FILE *file = fopen("output_potential.txt", "w");
        if (file == NULL) {
            fprintf(stderr, "Error: Could not open output_potential.txt for writing.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        for (i = 0; i < Nmesh; i++)
          for (j = 0; j < Nmesh; j++)
            for (k = 0; k < Nmesh; k++){
              coord = (i * Nmesh + j) * (2 * (Nmesh / 2 + 1)) + k;
              fprintf(file, "i= %d, j= %d, k= %d, pot= %.5e\n", i, j, k, pot_global[coord]); 
            }
        fclose(file);
      }
      free(pot_global);
    #endif

    if (ThisTask == 0){
      printf("-> Going back to Fourier space and finalizing output..\n");
      fflush(stdout);
    }

    // Go back to Fourier space, remove 1/N^3 factor, and set zero mode to zero
    MPI_Barrier(MPI_COMM_WORLD);
    rfftwnd_mpi(Forward_plan, 1, pot, Workspace, FFTW_NORMAL_ORDER);

    /* remove the N^3 I got by forwardfurier and put zero to zero mode */

    for (i = 0; i < Local_nx; i++)
      for (j = 0; j < Nmesh; j++)
        for (k = 0; k <= Nmesh / 2; k++)
        {
          coord = (i * Nmesh + j) * (Nmesh / 2 + 1) + k;
          // ****************************** DSJ *************************
          if (SphereMode == 1)
          {
            ii = i + Local_x_start;

            if (ii < Nmesh / 2)
              kvec[0] = ii * 2 * PI / Box;
            else
              kvec[0] = -(Nmesh - ii) * 2 * PI / Box;

            if (j < Nmesh / 2)
              kvec[1] = j * 2 * PI / Box;
            else
              kvec[1] = -(Nmesh - j) * 2 * PI / Box;

            if (k < Nmesh / 2)
              kvec[2] = k * 2 * PI / Box;
            else
              kvec[2] = -(Nmesh - k) * 2 * PI / Box;

            kmag = sqrt(kvec[0] * kvec[0] + kvec[1] * kvec[1] + kvec[2] * kvec[2]);

            if (kmag * Box / (2 * PI) > Nsample / 2)
            { /* select a sphere in k-space */
              cpot[coord].re = 0.;
              cpot[coord].im = 0.;
              continue;
            }
          }
          // ****************************** DSJ *************************
          cpot[coord].re /= (double)nmesh3;
          cpot[coord].im /= (double)nmesh3;
        }
    

    if (ThisTask == 0)
    {
      cpot[0].re = 0.;
      cpot[0].im = 0.;
    }

    if (ThisTask == 0 ) print_timed_done(1);





#else
// ********************** Collider Addition (End) ****************************
#ifdef EQUIL_FNL
  if (ThisTask == 0)
  {
    printf("Computing equilateral non-Gaussian potential...");
    fflush(stdout);
  };
#endif
#ifdef ORTOG_FNL
  if (ThisTask == 0)
  {
    printf("Computing orthogonal non-Gaussian potential... ");
    fflush(stdout);
  };
#endif

// ****  wrc ****
#ifdef ORTOG_LSS_FNL
  if (ThisTask == 0)
  {
    printf("Computing orthogonal-LSS non-Gaussian potential... ");
    fflush(stdout);
  };
#endif

  // ****  wrc ****
  /**** NON-LOCAL PRIMORDIAL POTENTIAL **************/

  /* allocate partpotential */

  cpartpot = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
  partpot = (fftw_real *)cpartpot;
  ASSERT_ALLOC(cpartpot);

  cp1p2p3sym = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
  p1p2p3sym = (fftw_real *)cp1p2p3sym;
  ASSERT_ALLOC(cp1p2p3sym);

  cp1p2p3sca = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
  p1p2p3sca = (fftw_real *)cp1p2p3sca;
  ASSERT_ALLOC(cp1p2p3sca);

  cp1p2p3nab = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
  p1p2p3nab = (fftw_real *)cp1p2p3nab;
  ASSERT_ALLOC(cp1p2p3nab);

  cp1p2p3tre = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
  p1p2p3tre = (fftw_real *)cp1p2p3tre;
  ASSERT_ALLOC(cp1p2p3tre);

// ****  wrc ****
#ifdef ORTOG_LSS_FNL
  cp1p2p3inv = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
  p1p2p3inv = (fftw_real *)cp1p2p3inv;
  ASSERT_ALLOC(cp1p2p3inv);
  cp1p2p3_K12D = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
  p1p2p3_K12D = (fftw_real *)cp1p2p3_K12D;
  ASSERT_ALLOC(cp1p2p3_K12D);
  cp1p2p3_K12E = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
  p1p2p3_K12E = (fftw_real *)cp1p2p3_K12E;
  ASSERT_ALLOC(cp1p2p3_K12E);
  cp1p2p3_K12F = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
  p1p2p3_K12F = (fftw_real *)cp1p2p3_K12F;
  ASSERT_ALLOC(cp1p2p3_K12F);
  cp1p2p3_K12G = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
  p1p2p3_K12G = (fftw_real *)cp1p2p3_K12G;
  ASSERT_ALLOC(cp1p2p3_K12G);
#endif
  // ****  wrc ****
  // ******************* DSJ ***********************
  exp_1_over_3 = (4. - PrimordialIndex) / 3.; // n_s modified exponent for generalized laplacian/inverse laplacian, exponent for k^2
  exp_1_over_6 = (4. - PrimordialIndex) / 6.; // n_s modified exponent for generalized conjugate gradient magnitude and its inverse, exponent for k^2
                                              // ******************* DSJ ***********************

  /* first, clean the array */
  for (i = 0; i < Local_nx; i++)
    for (j = 0; j < Nmesh; j++)
      for (k = 0; k <= Nmesh / 2; k++)
      {
        coord = (i * Nmesh + j) * (Nmesh / 2 + 1) + k;
        cp1p2p3sym[coord].re = 0;
        cp1p2p3sym[coord].im = 0;
        cp1p2p3sca[coord].re = 0;
        cp1p2p3sca[coord].im = 0;
        cp1p2p3nab[coord].re = 0;
        cp1p2p3nab[coord].im = 0;
        cp1p2p3tre[coord].re = 0;
        cp1p2p3tre[coord].im = 0;

// ****  wrc ****
#ifdef ORTOG_LSS_FNL
        cp1p2p3inv[coord].re = 0;
        cp1p2p3inv[coord].im = 0;
#endif
        // ****  wrc ****
        cpartpot[coord].re = 0;
        cpartpot[coord].im = 0;
      }

  /* multiply by k */

  for (ii = 0; ii < Local_nx; ii++)
    for (j = 0; j < Nmesh; j++)
      for (k = 0; k <= Nmesh / 2; k++)
      {

        coord = (ii * Nmesh + j) * (Nmesh / 2 + 1) + k;
        i = ii + Local_x_start;

        /* already are zero */
        /*  if(i == 0 && j == 0 && k == 0); continue */

        if (i < Nmesh / 2)
          kvec[0] = i * 2 * PI / Box;
        else
          kvec[0] = -(Nmesh - i) * 2 * PI / Box;

        if (j < Nmesh / 2)
          kvec[1] = j * 2 * PI / Box;
        else
          kvec[1] = -(Nmesh - j) * 2 * PI / Box;

        if (k < Nmesh / 2)
          kvec[2] = k * 2 * PI / Box;
        else
          kvec[2] = -(Nmesh - k) * 2 * PI / Box;

        kmag2 = kvec[0] * kvec[0] + kvec[1] * kvec[1] + kvec[2] * kvec[2];
        // ************************************ DSJ ********************************
        kmag_2_over_3 = pow(kmag2, exp_1_over_3);
        kmag_1_over_3 = pow(kmag2, exp_1_over_6);

        cpartpot[coord].re = kmag_1_over_3 * cpot[coord].re;
        cpartpot[coord].im = kmag_1_over_3 * cpot[coord].im;

        cp1p2p3nab[coord].re = kmag_2_over_3 * cpot[coord].re;
        cp1p2p3nab[coord].im = kmag_2_over_3 * cpot[coord].im;

// ****  wrc ****
#ifdef ORTOG_LSS_FNL
        if (i == 0 && j == 0 && k == 0)
        {
          continue;
        }
        cp1p2p3inv[coord].re = cpot[coord].re / kmag_1_over_3;
        cp1p2p3inv[coord].im = cpot[coord].im / kmag_1_over_3;
#endif
        // ****  wrc ****
        // ************************************ DSJ ********************************
      }

  MPI_Barrier(MPI_COMM_WORLD);

  /* Fourier back to real */
  rfftwnd_mpi(Inverse_plan, 1, pot, Workspace, FFTW_NORMAL_ORDER);
  rfftwnd_mpi(Inverse_plan, 1, partpot, Workspace, FFTW_NORMAL_ORDER);
  rfftwnd_mpi(Inverse_plan, 1, p1p2p3nab, Workspace, FFTW_NORMAL_ORDER);

// ****  wrc ****
#ifdef ORTOG_LSS_FNL
  rfftwnd_mpi(Inverse_plan, 1, p1p2p3inv, Workspace, FFTW_NORMAL_ORDER);
#endif
  // ****  wrc ****
  MPI_Barrier(MPI_COMM_WORLD);

  /* multiplying terms in real space  */

  for (i = 0; i < Local_nx; i++)
    for (j = 0; j < Nmesh; j++)
      for (k = 0; k < Nmesh; k++)
      {
        coord = (i * Nmesh + j) * (2 * (Nmesh / 2 + 1)) + k;

// ****  wrc ****
#ifdef ORTOG_LSS_FNL
        p1p2p3_K12D[coord] = p1p2p3inv[coord] * p1p2p3inv[coord]; // 1/k phi 1/k phi
        p1p2p3_K12E[coord] = p1p2p3nab[coord] * p1p2p3inv[coord]; // k^2 phi 1/k phi
        p1p2p3_K12F[coord] = pot[coord] * p1p2p3inv[coord];       // phi 1/k phi
        p1p2p3_K12G[coord] = partpot[coord] * p1p2p3inv[coord];   // k phi 1/k phk
#endif
        // ****  wrc ****
        p1p2p3sym[coord] = partpot[coord] * partpot[coord];
        p1p2p3sca[coord] = pot[coord] * partpot[coord];
        p1p2p3nab[coord] = pot[coord] * p1p2p3nab[coord];
        p1p2p3tre[coord] = p1p2p3nab[coord] * partpot[coord];
        partpot[coord] = pot[coord] * pot[coord]; /** NOTE: now partpot is potential squared **/
      }

  MPI_Barrier(MPI_COMM_WORLD);
  rfftwnd_mpi(Forward_plan, 1, pot, Workspace, FFTW_NORMAL_ORDER);
  rfftwnd_mpi(Forward_plan, 1, partpot, Workspace, FFTW_NORMAL_ORDER);
  rfftwnd_mpi(Forward_plan, 1, p1p2p3sym, Workspace, FFTW_NORMAL_ORDER);
  rfftwnd_mpi(Forward_plan, 1, p1p2p3sca, Workspace, FFTW_NORMAL_ORDER);
  rfftwnd_mpi(Forward_plan, 1, p1p2p3nab, Workspace, FFTW_NORMAL_ORDER);
  rfftwnd_mpi(Forward_plan, 1, p1p2p3tre, Workspace, FFTW_NORMAL_ORDER);

// ****  wrc ****
#ifdef ORTOG_LSS_FNL
  rfftwnd_mpi(Forward_plan, 1, p1p2p3_K12D, Workspace, FFTW_NORMAL_ORDER);
  rfftwnd_mpi(Forward_plan, 1, p1p2p3_K12E, Workspace, FFTW_NORMAL_ORDER);
  rfftwnd_mpi(Forward_plan, 1, p1p2p3_K12F, Workspace, FFTW_NORMAL_ORDER);
  rfftwnd_mpi(Forward_plan, 1, p1p2p3_K12G, Workspace, FFTW_NORMAL_ORDER);
#endif

  // ****  wrc ****
  MPI_Barrier(MPI_COMM_WORLD);

  /* divide by appropiate k's, sum terms according to non-local model */
  /* remove the N^3 I got by forwardfurier and put zero to zero mode */

  nmesh3 = ((unsigned int)Nmesh) * ((unsigned int)Nmesh) * ((unsigned int)Nmesh);

  for (ii = 0; ii < Local_nx; ii++)
    for (j = 0; j < Nmesh; j++)
      for (k = 0; k <= Nmesh / 2; k++)
      {

        coord = (ii * Nmesh + j) * (Nmesh / 2 + 1) + k;
        i = ii + Local_x_start;

        /* if(i == 0 && j == 0 && k == 0); continue; */

        if (i < Nmesh / 2)
          kvec[0] = i * 2 * PI / Box;
        else
          kvec[0] = -(Nmesh - i) * 2 * PI / Box;

        if (j < Nmesh / 2)
          kvec[1] = j * 2 * PI / Box;
        else
          kvec[1] = -(Nmesh - j) * 2 * PI / Box;

        if (k < Nmesh / 2)
          kvec[2] = k * 2 * PI / Box;
        else
          kvec[2] = -(Nmesh - k) * 2 * PI / Box;

        kmag2 = kvec[0] * kvec[0] + kvec[1] * kvec[1] + kvec[2] * kvec[2];
        // ****************************** DSJ *************************
        kmag_2_over_3 = pow(kmag2, exp_1_over_3);
        kmag_1_over_3 = pow(kmag2, exp_1_over_6);
        // ****************************** DSJ *************************

        if (i == 0 && j == 0 && k == 0)
        {
          cpot[0].re = 0.;
          cpot[0].im = 0.;
          continue;
        }

        // ****************************** DSJ *************************
        if (SphereMode == 1)
        {
          kmag = sqrt(kmag2);
          if (kmag * Box / (2 * PI) > Nsample / 2)
          { /* select a sphere in k-space */
            cpot[coord].re = 0.;
            cpot[coord].im = 0.;
            continue;
          }
        }
        // ****************************** DSJ *************************

#ifdef EQUIL_FNL
        /* fnl equilateral */
        // ****************************************************************************************** DSJ ********************************************************************************************************
        cpot[coord].re += Fnl * (-3.0 * cpartpot[coord].re - 2.0 * cp1p2p3sym[coord].re / kmag_2_over_3 + 4.0 * cp1p2p3sca[coord].re / kmag_1_over_3 + 2.0 * cp1p2p3nab[coord].re / kmag_2_over_3);
        cpot[coord].im += Fnl * (-3.0 * cpartpot[coord].im - 2.0 * cp1p2p3sym[coord].im / kmag_2_over_3 + 4.0 * cp1p2p3sca[coord].im / kmag_1_over_3 + 2.0 * cp1p2p3nab[coord].im / kmag_2_over_3);
// ****************************************************************************************** DSJ ********************************************************************************************************
#endif

#ifdef ORTOG_FNL
        // ****************************************************************************************** DSJ ********************************************************************************************************
        cpot[coord].re += Fnl * (-9.0 * cpartpot[coord].re - 8.0 * cp1p2p3sym[coord].re / kmag_2_over_3 + 10.0 * cp1p2p3sca[coord].re / kmag_1_over_3 + 8.0 * cp1p2p3nab[coord].re / kmag_2_over_3);
        cpot[coord].im += Fnl * (-9.0 * cpartpot[coord].im - 8.0 * cp1p2p3sym[coord].im / kmag_2_over_3 + 10.0 * cp1p2p3sca[coord].im / kmag_1_over_3 + 8.0 * cp1p2p3nab[coord].im / kmag_2_over_3);
// ****************************************************************************************** DSJ ********************************************************************************************************
#endif

// ****  wrc ****
#ifdef ORTOG_LSS_FNL
        cpot[coord].re += 3 * Fnl * (-(1.0 + 1.0 / 3.0 * orth_p) * cpartpot[coord].re - 1.0 / 3.0 * (2.0 + 20. / 9. * orth_p) * cp1p2p3sym[coord].re / kmag_2_over_3 + 2.0 * (1.0 + 5.0 / 9.0 * orth_p) * (1.0 - orth_t) * cp1p2p3sca[coord].re / kmag_1_over_3 + 2.0 * (1.0 + 5.0 / 9.0 * orth_p) * orth_t * cp1p2p3nab[coord].re / kmag_2_over_3);
        cpot[coord].re += 3 * Fnl * (orth_p / 27.0 * cp1p2p3_K12D[coord].re * kmag_2_over_3 - 20.0 * orth_p / 27.0 * cp1p2p3_K12E[coord].re / kmag_1_over_3 - 4.0 * orth_p / 9.0 * cp1p2p3_K12F[coord].re * kmag_1_over_3 + 10.0 / 9.0 * orth_p * cp1p2p3_K12G[coord].re);
        cpot[coord].im += 3 * Fnl * (-(1.0 + 1.0 / 3.0 * orth_p) * cpartpot[coord].im - 1.0 / 3.0 * (2.0 + 20. / 9. * orth_p) * cp1p2p3sym[coord].im / kmag_2_over_3 + 2.0 * (1.0 + 5.0 / 9.0 * orth_p) * (1.0 - orth_t) * cp1p2p3sca[coord].im / kmag_1_over_3 + 2.0 * (1.0 + 5.0 / 9.0 * orth_p) * orth_t * cp1p2p3nab[coord].im / kmag_2_over_3);
        cpot[coord].im += 3 * Fnl * (orth_p / 27.0 * cp1p2p3_K12D[coord].im * kmag_2_over_3 - 20.0 * orth_p / 27.0 * cp1p2p3_K12E[coord].im / kmag_1_over_3 - 4.0 * orth_p / 9.0 * cp1p2p3_K12F[coord].im * kmag_1_over_3 + 10.0 / 9.0 * orth_p * cp1p2p3_K12G[coord].im);
#endif
        // ****  wrc ****
        cpot[coord].re /= (double)nmesh3;
        cpot[coord].im /= (double)nmesh3;
      }

  free(cpartpot);
  free(cp1p2p3sym);
  free(cp1p2p3sca);
  free(cp1p2p3nab);
  free(cp1p2p3tre);
// ****  wrc ****p
#ifdef ORTOG_LSS_FNL
  free(cp1p2p3inv);
  free(cp1p2p3_K12D);
  free(cp1p2p3_K12E);
  free(cp1p2p3_K12F);
  free(cp1p2p3_K12G);
#endif
  // ****  wrc ****

  if (ThisTask == 0)
    print_timed_done(1);
#endif
#endif
#endif

  if (ThisTask == 0)
  {
    printf("Computing gradient of non-Gaussian potential...");
    fflush(stdout);
  };

  /****** FINISHED NON LOCAL POTENTIAL OR LOCAL FNL, STILL IN NONGAUSSIAN SECTION ****/
  /*****  Now 2LPT ****/
  // ARE WE ALIVE?
  for (axes = 0, bytes = 0; axes < 3; axes++)
  {
    cdisp[axes] = (fftw_complex *)malloc(bytes += sizeof(fftw_real) * TotalSizePlusAdditional);
    disp[axes] = (fftw_real *)cdisp[axes];
  }

  ASSERT_ALLOC(cdisp[0] && cdisp[1] && cdisp[2]);

#if defined(MULTICOMPONENTGLASSFILE) && defined(DIFFERENT_TRANSFER_FUNC)
  for (Type = MinType; Type <= MaxType; Type++)
#endif
  {

    /* first, clean the array */
    for (i = 0; i < Local_nx; i++)
      for (j = 0; j < Nmesh; j++)
        for (k = 0; k <= Nmesh / 2; k++)
          for (axes = 0; axes < 3; axes++)
          {
            cdisp[axes][(i * Nmesh + j) * (Nmesh / 2 + 1) + k].re = 0;
            cdisp[axes][(i * Nmesh + j) * (Nmesh / 2 + 1) + k].im = 0;
          }

    for (ii = 0; ii < Local_nx; ii++)
      for (j = 0; j < Nmesh; j++)
        for (k = 0; k <= Nmesh / 2; k++)
        {

          coord = (ii * Nmesh + j) * (Nmesh / 2 + 1) + k;
          i = ii + Local_x_start;

          /*   if(i == 0 && j == 0 && k == 0); continue; */

          if (i < Nmesh / 2)
            kvec[0] = i * 2 * PI / Box;
          else
            kvec[0] = -(Nmesh - i) * 2 * PI / Box;

          if (j < Nmesh / 2)
            kvec[1] = j * 2 * PI / Box;
          else
            kvec[1] = -(Nmesh - j) * 2 * PI / Box;

          if (k < Nmesh / 2)
            kvec[2] = k * 2 * PI / Box;
          else
            kvec[2] = -(Nmesh - k) * 2 * PI / Box;

          kmag2 = kvec[0] * kvec[0] + kvec[1] * kvec[1] + kvec[2] * kvec[2];
          kmag = sqrt(kmag2);

          t_of_k = TransferFunc(kmag);

          twb = t_of_k / Dplus / Beta;

          for (axes = 0; axes < 3; axes++)
          {
            cdisp[axes][coord].im = kvec[axes] * twb * cpot[coord].re;
            cdisp[axes][coord].re = -kvec[axes] * twb * cpot[coord].im;
          }
        }

    free(cpot);

    if (ThisTask == 0)
      print_timed_done(1);

#endif

    MPI_Barrier(MPI_COMM_WORLD);

    /* Compute displacement gradient */

    if (ThisTask == 0)
    {
      printf("Computing 2LPT potential...");
      fflush(stdout);
    };

    for (i = 0; i < 6; i++)
    {
      cdigrad[i] = (fftw_complex *)malloc(bytes = sizeof(fftw_real) * TotalSizePlusAdditional);
      digrad[i] = (fftw_real *)cdigrad[i];
      ASSERT_ALLOC(cdigrad[i]);
    }

    for (i = 0; i < Local_nx; i++)
      for (j = 0; j < Nmesh; j++)
        for (k = 0; k <= Nmesh / 2; k++)
        {
          coord = (i * Nmesh + j) * (Nmesh / 2 + 1) + k;
          if ((i + Local_x_start) < Nmesh / 2)
            kvec[0] = (i + Local_x_start) * 2 * PI / Box;
          else
            kvec[0] = -(Nmesh - (i + Local_x_start)) * 2 * PI / Box;

          if (j < Nmesh / 2)
            kvec[1] = j * 2 * PI / Box;
          else
            kvec[1] = -(Nmesh - j) * 2 * PI / Box;

          if (k < Nmesh / 2)
            kvec[2] = k * 2 * PI / Box;
          else
            kvec[2] = -(Nmesh - k) * 2 * PI / Box;

          /* Derivatives of ZA displacement  */
          /* d(dis_i)/d(q_j)  -> sqrt(-1) k_j dis_i */
          cdigrad[0][coord].re = -cdisp[0][coord].im * kvec[0]; /* disp0,0 */
          cdigrad[0][coord].im = cdisp[0][coord].re * kvec[0];

          cdigrad[1][coord].re = -cdisp[0][coord].im * kvec[1]; /* disp0,1 */
          cdigrad[1][coord].im = cdisp[0][coord].re * kvec[1];

          cdigrad[2][coord].re = -cdisp[0][coord].im * kvec[2]; /* disp0,2 */
          cdigrad[2][coord].im = cdisp[0][coord].re * kvec[2];

          cdigrad[3][coord].re = -cdisp[1][coord].im * kvec[1]; /* disp1,1 */
          cdigrad[3][coord].im = cdisp[1][coord].re * kvec[1];

          cdigrad[4][coord].re = -cdisp[1][coord].im * kvec[2]; /* disp1,2 */
          cdigrad[4][coord].im = cdisp[1][coord].re * kvec[2];

          cdigrad[5][coord].re = -cdisp[2][coord].im * kvec[2]; /* disp2,2 */
          cdigrad[5][coord].im = cdisp[2][coord].re * kvec[2];
        }

    for (i = 0; i < 6; i++)
      rfftwnd_mpi(Inverse_plan, 1, digrad[i], Workspace, FFTW_NORMAL_ORDER);

    /* Compute second order source and store it in digrad[3]*/

    for (i = 0; i < Local_nx; i++)
      for (j = 0; j < Nmesh; j++)
        for (k = 0; k < Nmesh; k++)
        {
          coord = (i * Nmesh + j) * (2 * (Nmesh / 2 + 1)) + k;

          digrad[3][coord] =

              digrad[0][coord] * (digrad[3][coord] + digrad[5][coord]) + digrad[3][coord] * digrad[5][coord] - digrad[1][coord] * digrad[1][coord] - digrad[2][coord] * digrad[2][coord] - digrad[4][coord] * digrad[4][coord];
        }

    rfftwnd_mpi(Forward_plan, 1, digrad[3], Workspace, FFTW_NORMAL_ORDER);

    /* The memory allocated for cdigrad[0], [1], and [2] will be used for 2nd order displacements */
    /* Freeing the rest. cdigrad[3] still has 2nd order displacement source, free later */

    for (axes = 0; axes < 3; axes++)
    {
      cdisp2[axes] = cdigrad[axes];
      disp2[axes] = (fftw_real *)cdisp2[axes];
    }

    free(cdigrad[4]);
    free(cdigrad[5]);

    /* Solve Poisson eq. and calculate 2nd order displacements */

    for (i = 0; i < Local_nx; i++)
      for (j = 0; j < Nmesh; j++)
        for (k = 0; k <= Nmesh / 2; k++)
        {
          coord = (i * Nmesh + j) * (Nmesh / 2 + 1) + k;
          if ((i + Local_x_start) < Nmesh / 2)
            kvec[0] = (i + Local_x_start) * 2 * PI / Box;
          else
            kvec[0] = -(Nmesh - (i + Local_x_start)) * 2 * PI / Box;

          if (j < Nmesh / 2)
            kvec[1] = j * 2 * PI / Box;
          else
            kvec[1] = -(Nmesh - j) * 2 * PI / Box;

          if (k < Nmesh / 2)
            kvec[2] = k * 2 * PI / Box;
          else
            kvec[2] = -(Nmesh - k) * 2 * PI / Box;

          kmag2 = kvec[0] * kvec[0] + kvec[1] * kvec[1] + kvec[2] * kvec[2];
#ifdef CORRECT_CIC
          /* calculate smooth factor for deconvolution of CIC interpolation */
          fx = fy = fz = 1;
          if (kvec[0] != 0)
          {
            fx = (kvec[0] * Box / 2) / Nmesh;
            fx = sin(fx) / fx;
          }
          if (kvec[1] != 0)
          {
            fy = (kvec[1] * Box / 2) / Nmesh;
            fy = sin(fy) / fy;
          }
          if (kvec[2] != 0)
          {
            fz = (kvec[2] * Box / 2) / Nmesh;
            fz = sin(fz) / fz;
          }
          ff = 1 / (fx * fy * fz);
          smth = ff * ff;
          /*  */
#endif

          /* cdisp2 = source * k / (sqrt(-1) k^2) */
          for (axes = 0; axes < 3; axes++)
          {
            if (kmag2 > 0.0)
            {
              cdisp2[axes][coord].re = cdigrad[3][coord].im * kvec[axes] / kmag2;
              cdisp2[axes][coord].im = -cdigrad[3][coord].re * kvec[axes] / kmag2;
            }
            else
              cdisp2[axes][coord].re = cdisp2[axes][coord].im = 0.0;
#ifdef CORRECT_CIC
            cdisp[axes][coord].re *= smth;
            cdisp[axes][coord].im *= smth;
            cdisp2[axes][coord].re *= smth;
            cdisp2[axes][coord].im *= smth;
#endif
          }
        }

    /* Free cdigrad[3] */
    free(cdigrad[3]);

    MPI_Barrier(MPI_COMM_WORLD);

    /* Now, both cdisp, and cdisp2 have the ZA and 2nd order displacements */

    for (axes = 0; axes < 3; axes++)
    {
      rfftwnd_mpi(Inverse_plan, 1, disp[axes], Workspace, FFTW_NORMAL_ORDER);
      rfftwnd_mpi(Inverse_plan, 1, disp2[axes], Workspace, FFTW_NORMAL_ORDER);

      /* now get the plane on the right side from neighbour on the right,
         and send the left plane */

      recvTask = ThisTask;
      do
      {
        recvTask--;
        if (recvTask < 0)
          recvTask = NTask - 1;
      } while (Local_nx_table[recvTask] == 0);

      sendTask = ThisTask;
      do
      {
        sendTask++;
        if (sendTask >= NTask)
          sendTask = 0;
      } while (Local_nx_table[sendTask] == 0);

      /* use non-blocking send */

      if (Local_nx > 0)
      {
        /* send ZA disp */
        MPI_Isend(&(disp[axes][0]),
                  sizeof(fftw_real) * Nmesh * (2 * (Nmesh / 2 + 1)),
                  MPI_BYTE, recvTask, 10, MPI_COMM_WORLD, &request);

        MPI_Recv(&(disp[axes][(Local_nx * Nmesh) * (2 * (Nmesh / 2 + 1))]),
                 sizeof(fftw_real) * Nmesh * (2 * (Nmesh / 2 + 1)),
                 MPI_BYTE, sendTask, 10, MPI_COMM_WORLD, &status);

        MPI_Wait(&request, &status);

        /* send 2nd order disp */
        MPI_Isend(&(disp2[axes][0]),
                  sizeof(fftw_real) * Nmesh * (2 * (Nmesh / 2 + 1)),
                  MPI_BYTE, recvTask, 10, MPI_COMM_WORLD, &request);

        MPI_Recv(&(disp2[axes][(Local_nx * Nmesh) * (2 * (Nmesh / 2 + 1))]),
                 sizeof(fftw_real) * Nmesh * (2 * (Nmesh / 2 + 1)),
                 MPI_BYTE, sendTask, 10, MPI_COMM_WORLD, &status);

        MPI_Wait(&request, &status);
      }
    }

    if (ThisTask == 0)
      print_timed_done(21);
    if (ThisTask == 0)
    {
      printf("Computing displacements and velocitites...");
      fflush(stdout);
    };

    /* read-out displacements */
    nmesh3 = Nmesh * Nmesh * Nmesh;
    for (n = 0; n < NumPart; n++)
    {
#if defined(MULTICOMPONENTGLASSFILE) && defined(DIFFERENT_TRANSFER_FUNC)
      if (P[n].Type == Type)
#endif
      {
        u = P[n].Pos[0] / Box * Nmesh;
        v = P[n].Pos[1] / Box * Nmesh;
        w = P[n].Pos[2] / Box * Nmesh;

        i = (int)u;
        j = (int)v;
        k = (int)w;

        if (i == (Local_x_start + Local_nx))
          i = (Local_x_start + Local_nx) - 1;
        if (i < Local_x_start)
          i = Local_x_start;
        if (j == Nmesh)
          j = Nmesh - 1;
        if (k == Nmesh)
          k = Nmesh - 1;

        u -= i;
        v -= j;
        w -= k;

        i -= Local_x_start;
        ii = i + 1;
        jj = j + 1;
        kk = k + 1;

        if (jj >= Nmesh)
          jj -= Nmesh;
        if (kk >= Nmesh)
          kk -= Nmesh;

        f1 = (1 - u) * (1 - v) * (1 - w);
        f2 = (1 - u) * (1 - v) * (w);
        f3 = (1 - u) * (v) * (1 - w);
        f4 = (1 - u) * (v) * (w);
        f5 = (u) * (1 - v) * (1 - w);
        f6 = (u) * (1 - v) * (w);
        f7 = (u) * (v) * (1 - w);
        f8 = (u) * (v) * (w);

        for (axes = 0; axes < 3; axes++)
        {
          dis = disp[axes][(i * Nmesh + j) * (2 * (Nmesh / 2 + 1)) + k] * f1 +
                disp[axes][(i * Nmesh + j) * (2 * (Nmesh / 2 + 1)) + kk] * f2 +
                disp[axes][(i * Nmesh + jj) * (2 * (Nmesh / 2 + 1)) + k] * f3 +
                disp[axes][(i * Nmesh + jj) * (2 * (Nmesh / 2 + 1)) + kk] * f4 +
                disp[axes][(ii * Nmesh + j) * (2 * (Nmesh / 2 + 1)) + k] * f5 +
                disp[axes][(ii * Nmesh + j) * (2 * (Nmesh / 2 + 1)) + kk] * f6 +
                disp[axes][(ii * Nmesh + jj) * (2 * (Nmesh / 2 + 1)) + k] * f7 +
                disp[axes][(ii * Nmesh + jj) * (2 * (Nmesh / 2 + 1)) + kk] * f8;

          dis2 = disp2[axes][(i * Nmesh + j) * (2 * (Nmesh / 2 + 1)) + k] * f1 +
                 disp2[axes][(i * Nmesh + j) * (2 * (Nmesh / 2 + 1)) + kk] * f2 +
                 disp2[axes][(i * Nmesh + jj) * (2 * (Nmesh / 2 + 1)) + k] * f3 +
                 disp2[axes][(i * Nmesh + jj) * (2 * (Nmesh / 2 + 1)) + kk] * f4 +
                 disp2[axes][(ii * Nmesh + j) * (2 * (Nmesh / 2 + 1)) + k] * f5 +
                 disp2[axes][(ii * Nmesh + j) * (2 * (Nmesh / 2 + 1)) + kk] * f6 +
                 disp2[axes][(ii * Nmesh + jj) * (2 * (Nmesh / 2 + 1)) + k] * f7 +
                 disp2[axes][(ii * Nmesh + jj) * (2 * (Nmesh / 2 + 1)) + kk] * f8;
          dis2 /= (float)nmesh3;

#ifdef ONLY_ZA
          P[n].Pos[axes] += dis;
          P[n].Vel[axes] = dis * vel_prefac;
#else
          P[n].Pos[axes] += dis - 3. / 7. * dis2;
          P[n].Vel[axes] = dis * vel_prefac - 3. / 7. * dis2 * vel_prefac2;
#endif

          P[n].Pos[axes] = periodic_wrap(P[n].Pos[axes]);

          if (fabs(dis - 3. / 7. * dis2 > maxdisp))
            maxdisp = fabs(dis - 3. / 7. * dis2);
        }
      }
    }
  }

  if (ThisTask == 0)
    print_timed_done(6);

  for (axes = 0; axes < 3; axes++)
    free(cdisp[axes]);
  for (axes = 0; axes < 3; axes++)
    free(cdisp2[axes]);

  gsl_rng_free(random_generator);

  MPI_Reduce(&maxdisp, &max_disp_glob, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

  /*  if(ThisTask == 0)
      {
        printf("\nMaximum displacement (1D): %g kpc/h, in units of the part-spacing= %g\n",
         max_disp_glob, max_disp_glob / (Box / Nmesh));
      }*/
}

double periodic_wrap(double x)
{
  while (x >= Box)
    x -= Box;

  while (x < 0)
    x += Box;

  return x;
}

void set_units(void) /* ... set some units */
{
  UnitTime_in_s = UnitLength_in_cm / UnitVelocity_in_cm_per_s;
  G = GRAVITY / pow(UnitLength_in_cm, 3) * UnitMass_in_g * pow(UnitTime_in_s, 2);
  Hubble = HUBBLE * UnitTime_in_s;
}

void initialize_ffts(void)
{
  int total_size, i, additional;
  int local_ny_after_transpose, local_y_start_after_transpose;
  int *slab_to_task_local;
  size_t bytes;

  Inverse_plan = rfftw3d_mpi_create_plan(MPI_COMM_WORLD,
                                         Nmesh, Nmesh, Nmesh, FFTW_COMPLEX_TO_REAL, FFTW_ESTIMATE);

  Forward_plan = rfftw3d_mpi_create_plan(MPI_COMM_WORLD,
                                         Nmesh, Nmesh, Nmesh, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);

  rfftwnd_mpi_local_sizes(Forward_plan, &Local_nx, &Local_x_start,
                          &local_ny_after_transpose, &local_y_start_after_transpose, &total_size);

  Local_nx_table = malloc(sizeof(int) * NTask);
  MPI_Allgather(&Local_nx, 1, MPI_INT, Local_nx_table, 1, MPI_INT, MPI_COMM_WORLD);

  Slab_to_task = malloc(sizeof(int) * Nmesh);
  slab_to_task_local = malloc(sizeof(int) * Nmesh);

  for (i = 0; i < Nmesh; i++)
    slab_to_task_local[i] = 0;

  for (i = 0; i < Local_nx; i++)
    slab_to_task_local[Local_x_start + i] = ThisTask;

  MPI_Allreduce(slab_to_task_local, Slab_to_task, Nmesh, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

  free(slab_to_task_local);

  additional = (Nmesh) * (2 * (Nmesh / 2 + 1)); /* additional plane on the right side */

  TotalSizePlusAdditional = total_size + additional;

  Workspace = (fftw_real *)malloc(bytes = sizeof(fftw_real) * total_size);

  ASSERT_ALLOC(Workspace)

  #ifdef OUTPUT_DF
    // set the size of the coordinates and density field arrays
    coord_DF      = malloc(sizeof(long long)*Local_nx*Nmesh*(Nmesh/2 + 1));
    amplitudes = malloc(sizeof(float)*Local_nx*Nmesh*(Nmesh/2 + 1)); 
    phases     = malloc(sizeof(float)*Local_nx*Nmesh*(Nmesh/2 + 1)); 


    if (coord_DF && amplitudes && phases) 
      if (ThisTask==0)
        printf("Memory allocated for coordinates, amplitudes and phases\n");
  #endif

}

void free_ffts(void)
{
  free(Workspace);
  free(Slab_to_task);
  rfftwnd_mpi_destroy_plan(Inverse_plan);
  rfftwnd_mpi_destroy_plan(Forward_plan);
}

int FatalError(int errnum)
{
  printf("FatalError called with number=%d\n", errnum);
  fflush(stdout);
  MPI_Abort(MPI_COMM_WORLD, errnum);
  exit(0);
}

static double A, B, alpha, beta, V, gf;

double fnl(double x) /* Peacock & Dodds formula */
{
  return x * pow((1 + B * beta * x + pow(A * x, alpha * beta)) /
                     (1 + pow(pow(A * x, alpha) * gf * gf * gf / (V * sqrt(x)), beta)),
                 1 / beta);
}

void print_spec(void)
{
  double k, knl, po, dl, dnl, neff, kf, kstart, kend, po2, po1, DDD;
  char buf[1000];
  FILE *fd;

  if (ThisTask == 0)
  {
    sprintf(buf, "%s/inputspec_%s.txt", OutputDir, FileBase);

    fd = fopen(buf, "w");

    gf = GrowthFactor(0.001, 1.0) / (1.0 / 0.001);

    DDD = GrowthFactor(1.0 / (Redshift + 1), 1.0);

    fprintf(fd, "%12g %12g\n", Redshift, DDD); /* print actual starting redshift and
              linear growth factor for this cosmology */

    kstart = 2 * PI / (1000.0 * (3.085678e24 / UnitLength_in_cm)); /* 1000 Mpc/h */
    kend = 2 * PI / (0.001 * (3.085678e24 / UnitLength_in_cm));    /* 0.001 Mpc/h */

    for (k = kstart; k < kend; k *= 1.025)
    {
      po = PowerSpec(k);
      dl = 4.0 * PI * k * k * k * po;

      kf = 0.5;

      po2 = PowerSpec(1.001 * k * kf);
      po1 = PowerSpec(k * kf);

      if (po != 0 && po1 != 0 && po2 != 0)
      {
        neff = (log(po2) - log(po1)) / (log(1.001 * k * kf) - log(k * kf));

        if (1 + neff / 3 > 0)
        {
          A = 0.482 * pow(1 + neff / 3, -0.947);
          B = 0.226 * pow(1 + neff / 3, -1.778);
          alpha = 3.310 * pow(1 + neff / 3, -0.244);
          beta = 0.862 * pow(1 + neff / 3, -0.287);
          V = 11.55 * pow(1 + neff / 3, -0.423) * 1.2;

          dnl = fnl(dl);
          knl = k * pow(1 + dnl, 1.0 / 3);
        }
        else
        {
          dnl = 0;
          knl = 0;
        }
      }
      else
      {
        dnl = 0;
        knl = 0;
      }

      fprintf(fd, "%12g %12g    %12g %12g\n", k, dl, knl, dnl);
    }
    fclose(fd);
  }
}
