#ifndef VISIT_HPP
#define VISIT_HPP 1

#include <vector>
#include "hdf5.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////
//h5visit_t
/////////////////////////////////////////////////////////////////////////////////////////////////////

struct H5O_info_added_t
{
  H5O_info_t oinfo;
  size_t added;
};
  

class h5visit_t
{
public:
  h5visit_t()
  {
  }

  // visit
  int visit(const char* file_name);

  // HDF5 object information
  std::vector < H5O_info_added_t > visit_info;

private:
  // callback function for H5Lvisit_by_name
  static herr_t visit_link_cb(hid_t loc_id, const char *name, const H5L_info_t *linfo, void *_op_data);
};

#endif