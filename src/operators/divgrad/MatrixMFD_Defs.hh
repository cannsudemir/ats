/*
  License: BSD
  Authors: Konstantin Lipnikov (version 2) (lipnikov@lanl.gov)
           Ethan Coon (ecoon@lanl.gov) (ATS version)
  MatrixMFD provides a mimetic discretization for the elliptic operator div K grad u.

*/

#ifndef OPERATORS_MATRIX_MFD_DEFS_HH_
#define OPERATORS_MATRIX_MFD_DEFS_HH_

namespace Amanzi {
namespace Operators {

const int MFD_HEX_FACES = 6;  // Hexahedron is the common element
const int MFD_HEX_NODES = 8;
const int MFD_HEX_EDGES = 12;

const int MFD_QUAD_FACES = 4;  // Quadrilateral is the common element
const int MFD_QUAD_NODES = 4;
const int MFD_QUAD_EDGES = 4;

const int MFD_MAX_FACES = 14;  // Kelvin's tetrakaidecahedron
const int MFD_MAX_NODES = 47;  // These polyhedron parameters must
const int MFD_MAX_EDGES = 60;  // be calculated in Init().

enum MatrixBC {
  MATRIX_BC_NULL = 0,
  MATRIX_BC_DIRICHLET,
  MATRIX_BC_FLUX
};

enum MFDMethod {
  MFD3D_NULL = 0,
  MFD3D_POLYHEDRA,
  MFD3D_POLYHEDRA_SCALED,
  MFD3D_OPTIMIZED,
  MFD3D_OPTIMIZED_SCALED,
  MFD3D_HEXAHEDRA_MONOTONE,
  MFD3D_TWO_POINT_FLUX,
  MFD3D_SUPPORT_OPERATOR
};


} // namespace
} // namespace


#endif
