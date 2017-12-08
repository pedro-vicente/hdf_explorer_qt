
#include <QtDebug>
#include "visit.hpp"

/////////////////////////////////////////////////////////////////////////////////////////////////////
//h5visit_t::visit
/////////////////////////////////////////////////////////////////////////////////////////////////////

int h5visit_t::visit(const char* file_name)
{
  hid_t fid;

  if((fid = H5Fopen(file_name, H5F_ACC_RDONLY, H5P_DEFAULT)) < 0)
  {
    return -1;
  }

  h5visit_t *udata;
  udata = this;

  // user data for iteration callback 
  // store the "this" pointer to allow the static member function "visit_link_cb" to call class members
  // callback function 'visit_link_cb' must be a static member function

  if(H5Lvisit_by_name(fid, "/", H5_INDEX_NAME, H5_ITER_INC, visit_link_cb, udata, H5P_DEFAULT) < 0)
  {
    return -1;
  }

  if(H5Fclose(fid) < 0)
  {

  }

  return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//h5visit_t::visit_link_cb
/////////////////////////////////////////////////////////////////////////////////////////////////////

herr_t h5visit_t::visit_link_cb(hid_t loc_id, const char *name, const H5L_info_t *linfo, void *_op_data)
{
  h5visit_t *udata = (h5visit_t*)_op_data;

  // user data for iteration callback 
  // udata is "this"

  //hard link
  if(linfo->type == H5L_TYPE_HARD)
  {
    // information struct for link
    H5O_info_t oinfo;

    // get information about the object 
    if(H5Oget_info_by_name(loc_id, name, &oinfo, H5P_DEFAULT) < 0)
    {
      return(H5_ITER_ERROR);
    }

    H5O_info_added_t oinfo_added;
    oinfo_added.oinfo = oinfo;
    oinfo_added.added = 0;

    udata->visit_info.push_back(oinfo_added);

    qDebug() << name;

  }

  return(H5_ITER_CONT);
}


