#ifndef ITERATE_HPP
#define ITERATE_HPP 1

#include <vector>
#include "hdf5.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////
//h5iterate_t
/////////////////////////////////////////////////////////////////////////////////////////////////////

class h5iterate_t
{
public:
  h5iterate_t()
  {
  }

  // iterate
  int iterate(const char* file_name);

  // HDF5 object information
  std::vector < H5O_info_t > iterate_info;

private:
  // callback function for H5Lvisit_by_name
  static herr_t iterate_link_cb(hid_t loc_id, const char *name, const H5L_info_t *linfo, void *_op_data);
};

#endif