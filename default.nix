{ stdenv, fetchFromGitHub, fetchpatch, unzip, cmake, python, pkgconfig, gtest, hdf5, lz4 }:

stdenv.mkDerivation {
  pname = "flann";
  version = "1.9.1";

  src = ./.;

  nativeBuildInputs = [ cmake pkgconfig ];
  buildInputs = [ python gtest hdf5 lz4 ];

  meta = {
    homepage = http://people.cs.ubc.ca/~mariusm/flann/;
    license = stdenv.lib.licenses.bsd3;
    description = "Fast approximate nearest neighbor searches in high dimensional spaces";
  };
}
