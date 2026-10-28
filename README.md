# Hw 2: Wisps

Videos for wisps, pyro, and noise are in 
images/wisps -> wisps
images/pyro_wedges -> pyro
images/wedges -> noise

look for .mov files for the videos!

noise parameters:
std::vector<float> mins = {1, 0.01, 0.00, 0.01, 1e-6};
std::vector<float> maxes = {8, 4, 1, 5, 3};
std::vector<float> defaults = {2.5, 1.5, 0.5, 2, 0};
std::vector<InterpFunc> interps = {lerp, lerp, lerp, lerp, lerp};
std::vector<std::string> names = {"octave", "fade", "roughness", "freq", "fjump"};

pyro parameters:
std::vector<float> mins = {1, 0.01, 0.01, 1e-6, 0.01};
std::vector<float> maxes = {8, 5, 5, 5, 5};
std::vector<float> defaults = {3, 1, 2, 2, 2};
std::vector<InterpFunc> interps = {lerp, lerp, lerp, lerp, lerp};
std::vector<std::string> names = {"octave", "gamma", "freq", "fjump", "amp"};

wisp parameters: (for wisp_out_v02.mov)
std::vector<float> mins = {0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.1, 0.1};
std::vector<float> maxes = {10, 10, 10, 10, 5, 5, 5, 5, 5, 1, 1};
std::vector<float> defaults = {0.75, 1, 3, 5, 2, 1, 2, 1.5, 1, 0.1, 0.2};
std::vector<InterpFunc> interps = {lerp, lerp, lerp, lerp, lerp, lerp, lerp, lerp, lerp, lerp, lerp};
std::vector<std::string> names = {"clump", "pscale", "o1", "o2", "f1" "f2", "fj1", "fj2", "dips", "rough1", "rough2"};


Most of the code is in in the /include and /src folders.


### Some extra info
The project is built with cmake, hence the build folder and all that goes with it. The third_party folder
currently contains the code for the image processing library from Computer Graphics Images, along with 
the starter kit code. 




