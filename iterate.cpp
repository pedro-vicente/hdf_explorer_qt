
#include <QtDebug>
#include "iterate.hpp"

/////////////////////////////////////////////////////////////////////////////////////////////////////
//h5iterate_t::iterate
/////////////////////////////////////////////////////////////////////////////////////////////////////

int h5iterate_t::iterate(const char* file_name)
{
  hid_t fid;

  if((fid = H5Fopen(file_name, H5F_ACC_RDONLY, H5P_DEFAULT)) < 0)
  {
    return -1;
  }

  h5iterate_t *udata;
  udata = this;

  // user data for iteration callback 
  // store the "this" pointer to allow the static member function "visit_link_cb" to call class members
  // callback function 'visit_link_cb' must be a static member function

  if(H5Literate(fid, H5_INDEX_NAME, H5_ITER_INC, NULL, iterate_link_cb, udata) < 0)
  {
    return -1;
  }

  if(H5Fclose(fid) < 0)
  {

  }

  return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//h5iterate_t::iterate_link_cb
/////////////////////////////////////////////////////////////////////////////////////////////////////

herr_t h5iterate_t::iterate_link_cb(hid_t loc_id, const char *name, const H5L_info_t *linfo, void *_op_data)
{
  h5iterate_t *udata = (h5iterate_t*)_op_data;
  hid_t did;
  hid_t gid;

  // user data for iteration callback 
  // udata is "this"

  //hard link
  if(linfo->type == H5L_TYPE_HARD)
  {
    H5O_info_t oinfo;

    // get information about the object 
    if(H5Oget_info_by_name(loc_id, name, &oinfo, H5P_DEFAULT) < 0)
    {
      return(H5_ITER_ERROR);
    }

    udata->iterate_info.push_back(oinfo);

    qDebug() << name;

    switch(oinfo.type)
    {

    case H5G_GROUP:

      if((gid = H5Gopen2(loc_id, name, H5P_DEFAULT))<0)
      {

      }

      if(H5Literate(gid, H5_INDEX_NAME, H5_ITER_INC, NULL, iterate_link_cb, udata) < 0)
      {
        return -1;
      }

      if(H5Gclose(gid)<0)
      {

      }

      break;

    case H5G_DATASET:

      if((did = H5Dopen2(loc_id, name, H5P_DEFAULT))<0)
      {

      }

      if(H5Dclose(did)<0)
      {

      }

      break;

    case H5G_TYPE:

      break;
    default:

      break;
    }

  }

  return(H5_ITER_CONT);
}
