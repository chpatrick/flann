{ gccStdenv, overrideCC, lib, fetchpatch, unzip, cmake, python, pkgconfig, gtest, hdf5, lz4, cudatoolkit }:

(overrideCC gccStdenv cudatoolkit.cc).mkDerivation {
  pname = "flann";
  version = "1.9.1";

  src = ./.;

  nativeBuildInputs = [ cmake pkgconfig ];
  buildInputs = [ python gtest hdf5 lz4 cudatoolkit ];
  cmakeFlags = [
    "-DBUILD_CUDA_LIB=ON"
  ];

  meta = {
    homepage = http://people.cs.ubc.ca/~mariusm/flann/;
    license = lib.licenses.bsd3;
    description = "Fast approximate nearest neighbor searches in high dimensional spaces";
  };
}
