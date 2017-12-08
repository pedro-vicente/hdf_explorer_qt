//Copyright (C) 2016 Pedro Vicente
//GNU General Public License (GPL) Version 3 described in the LICENSE file 

#include <QApplication>
#include <QMetaType>
#include <QtGui>
#include <QIcon>
#include <QMdiArea>
#include <QtDebug>
#include <string>
#include <cassert>
#include <vector>
#include <algorithm>
#include "hdf_explorer.hpp"

static const char app_name[] = "HDF Explorer";

/////////////////////////////////////////////////////////////////////////////////////////////////////
//main
/////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
  Q_INIT_RESOURCE(hdf_explorer);
  QApplication app(argc, argv);
  QCoreApplication::setApplicationVersion("1.0");
  QCoreApplication::setApplicationName(app_name);
#if QT_VERSION >= 0x050000
  QCommandLineParser parser;
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument("file", "The file to open.");
  parser.process(app);
  const QStringList args = parser.positionalArguments();
#endif

  MainWindow window;
#if QT_VERSION >= 0x050000
  if(args.size())
  {
    QString file_name = args.at(0);
    window.read_file(file_name);
  }
#endif
  window.showMaximized();
  return app.exec();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//hdf_dataset_t (common definition for HDF5 dataset and attribute)
//a HDF dataset is defined here simply has a:
// 1) location in memory to store dataset data
// 2) an array of dimensions
// 3) a full name path to open using a file id
// 4) size, sign and class of datatype
// the array of dimensions (of HDF defined type ' hsize_t') is defined in iteration
// the data buffer and datatype sizes are stored on per load variable 
// from tree using the HDF API from item input
/////////////////////////////////////////////////////////////////////////////////////////////////////

class hdf_dataset_t
{
public:
  hdf_dataset_t(const char* path, const std::vector< hsize_t> &dim,
    size_t size, H5T_sign_t sign, H5T_class_t datatype_class) :
    m_path(path),
    m_dim(dim),
    m_datatype_size(size),
    m_datatype_sign(sign),
    m_datatype_class(datatype_class)
  {
    m_buf = NULL;
  }

  ~hdf_dataset_t()
  {
    free(m_buf);
  }

  void store(void *buf)
  {
    m_buf = buf;
  }
  std::string m_path;
  std::vector<hsize_t> m_dim;

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  //needed to access HDF5 buffer data
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  size_t m_datatype_size;
  H5T_sign_t m_datatype_sign;
  H5T_class_t  m_datatype_class;
  void *m_buf;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
//ItemData
//used by QTreeWidgetItem::setData to store custom data
//contains information to load a dataset from an HDF file:
//1) the file name
//2) the dataset name
//3) that's all :-)
/////////////////////////////////////////////////////////////////////////////////////////////////////

class ItemData
{
public:
  enum ItemKind
  {
    Root,
    Group,
    Variable,
    Attribute
  };

  ItemData(ItemKind kind, const std::string& file_name, const std::string& item_nm, hdf_dataset_t *dataset) :
    m_file_name(file_name),
    m_item_nm(item_nm),
    m_kind(kind),
    m_dataset(dataset)
  {
  }
  ~ItemData()
  {
    delete m_dataset;
  }
  std::string m_file_name;  // (Root/Variable/Group/Attribute) file name
  std::string m_item_nm; // (Root/Variable/Group/Attribute ) item name to display on tree
  ItemKind m_kind; // (Root/Variable/Group/Attribute) type of item 
  hdf_dataset_t *m_dataset; // (Variable) HDF variable to display
};

Q_DECLARE_METATYPE(ItemData*);

/////////////////////////////////////////////////////////////////////////////////////////////////////
//get_item_data
/////////////////////////////////////////////////////////////////////////////////////////////////////

ItemData* get_item_data(QTreeWidgetItem *item)
{
  QVariant data = item->data(0, Qt::UserRole);
  ItemData *item_data = data.value<ItemData*>();
  return item_data;
}

///////////////////////////////////////////////////////////////////////////////////////
//name_type_t
//struct to get name and type of an object
///////////////////////////////////////////////////////////////////////////////////////

struct name_type_t
{
  char *name;
  int type;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
//MainWindow::MainWindow
/////////////////////////////////////////////////////////////////////////////////////////////////////

MainWindow::MainWindow()
{
  ///////////////////////////////////////////////////////////////////////////////////////
  //mdi area
  ///////////////////////////////////////////////////////////////////////////////////////

  m_mdi_area = new QMdiArea;
  setCentralWidget(m_mdi_area);

  setWindowTitle(tr(app_name));

  ///////////////////////////////////////////////////////////////////////////////////////
  //status bar
  ///////////////////////////////////////////////////////////////////////////////////////

  statusBar()->showMessage(tr("Ready"));

  ///////////////////////////////////////////////////////////////////////////////////////
  //dock for tree
  ///////////////////////////////////////////////////////////////////////////////////////

  m_tree_dock = new QDockWidget(this);
  m_tree_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);

  ///////////////////////////////////////////////////////////////////////////////////////
  //browser tree
  ///////////////////////////////////////////////////////////////////////////////////////

  m_tree = new FileTreeWidget();
  m_tree->setHeaderHidden(1);
  m_tree->set_main_window(this);
  QStringList str_style = QStyleFactory::keys();
  qDebug() << str_style;
  for(int i = 0; i < str_style.size(); ++i)
  {
    if("Windows" == str_style[i])
    {
      QStyle *style = QStyleFactory::create(str_style[i]);
      m_tree->setStyle(style);
    }
  }
  //add dock
  m_tree_dock->setWidget(m_tree);
  addDockWidget(Qt::LeftDockWidgetArea, m_tree_dock);

  ///////////////////////////////////////////////////////////////////////////////////////
  //actions
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  //open
  ///////////////////////////////////////////////////////////////////////////////////////

  m_action_open = new QAction(tr("&Open..."), this);
  m_action_open->setIcon(QIcon(":/images/open.png"));
  m_action_open->setShortcut(QKeySequence::Open);
  m_action_open->setStatusTip(tr("Open a file"));
  connect(m_action_open, SIGNAL(triggered()), this, SLOT(open_file()));

  ///////////////////////////////////////////////////////////////////////////////////////
  //exit
  ///////////////////////////////////////////////////////////////////////////////////////

  m_action_exit = new QAction(tr("E&xit"), this);
  m_action_exit->setShortcut(tr("Ctrl+Q"));
  m_action_exit->setStatusTip(tr("Exit the application"));
  connect(m_action_exit, SIGNAL(triggered()), this, SLOT(close()));

  ///////////////////////////////////////////////////////////////////////////////////////
  //windows
  ///////////////////////////////////////////////////////////////////////////////////////

  m_action_close_all = new QAction(tr("Close &All"), this);
  m_action_close_all->setStatusTip(tr("Close all the windows"));
  connect(m_action_close_all, SIGNAL(triggered()), m_mdi_area, SLOT(closeAllSubWindows()));

  m_action_tile = new QAction(tr("&Tile"), this);
  m_action_tile->setStatusTip(tr("Tile the windows"));
  connect(m_action_tile, SIGNAL(triggered()), m_mdi_area, SLOT(tileSubWindows()));

  ///////////////////////////////////////////////////////////////////////////////////////
  //about
  ///////////////////////////////////////////////////////////////////////////////////////

  m_action_about = new QAction(tr("&About"), this);
  m_action_about->setStatusTip(tr("Show the application's About box"));
  connect(m_action_about, SIGNAL(triggered()), this, SLOT(about()));

  ///////////////////////////////////////////////////////////////////////////////////////
  //recent files
  ///////////////////////////////////////////////////////////////////////////////////////

  for(int i = 0; i < max_recent_files; ++i)
  {
    m_action_recent_file[i] = new QAction(this);
    m_action_recent_file[i]->setVisible(false);
    connect(m_action_recent_file[i], SIGNAL(triggered()), this, SLOT(open_recent_file()));
  }

  ///////////////////////////////////////////////////////////////////////////////////////
  //menus
  ///////////////////////////////////////////////////////////////////////////////////////

  m_menu_file = menuBar()->addMenu(tr("&File"));
  m_menu_file->addAction(m_action_open);
  m_action_separator_recent = m_menu_file->addSeparator();
  for(int i = 0; i < max_recent_files; ++i)
  {
    m_menu_file->addAction(m_action_recent_file[i]);
  }
  m_menu_file->addSeparator();
  m_menu_file->addAction(m_action_exit);

  m_menu_windows = menuBar()->addMenu(tr("&Window"));
  m_menu_windows->addAction(m_action_tile);
  m_menu_windows->addAction(m_action_close_all);

  m_menu_help = menuBar()->addMenu(tr("&Help"));
  m_menu_help->addAction(m_action_about);

  ///////////////////////////////////////////////////////////////////////////////////////
  //toolbar
  ///////////////////////////////////////////////////////////////////////////////////////

  m_tool_bar = addToolBar(tr("&File"));
  m_tool_bar->addAction(m_action_open);

  //avoid popup on toolbar
  setContextMenuPolicy(Qt::NoContextMenu);

  ///////////////////////////////////////////////////////////////////////////////////////
  //settings
  ///////////////////////////////////////////////////////////////////////////////////////

  QSettings settings("space", "hdf_explorer");
  m_sl_recent_files = settings.value("recentFiles").toStringList();
  update_recent_file_actions();

  ///////////////////////////////////////////////////////////////////////////////////////
  //icons
  ///////////////////////////////////////////////////////////////////////////////////////

  m_icon_main = QIcon(":/images/sample.png");
  m_icon_group = QIcon(":/images/folder.png");
  m_icon_dataset = QIcon(":/images/matrix.png");
  m_icon_attribute = QIcon(":/images/document.png");
  m_icon_image_indexed = QIcon(":/images/image_indexed.png");
  m_icon_image_true = QIcon(":/images/image_true.png");

  ///////////////////////////////////////////////////////////////////////////////////////
  //set main window icon
  ///////////////////////////////////////////////////////////////////////////////////////

  setWindowIcon(m_icon_main);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//MainWindow::about
/////////////////////////////////////////////////////////////////////////////////////////////////////

void MainWindow::about()
{
  QMessageBox::about(this,
    tr("About HDF Explorer"),
    tr("(c) 2015-2016 Pedro Vicente -- Space Research Software LLC\n\n"));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//MainWindow::closeEvent
/////////////////////////////////////////////////////////////////////////////////////////////////////

void MainWindow::closeEvent(QCloseEvent *eve)
{
  QSettings settings("space", "hdf_explorer");
  settings.setValue("recentFiles", m_sl_recent_files);
  eve->accept();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//last_component
/////////////////////////////////////////////////////////////////////////////////////////////////////

QString last_component(const QString &full_file_name)
{
  return QFileInfo(full_file_name).fileName();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//MainWindow::update_recent_file_actions
/////////////////////////////////////////////////////////////////////////////////////////////////////

void MainWindow::update_recent_file_actions()
{
  QMutableStringListIterator i(m_sl_recent_files);
  while(i.hasNext())
  {
    QString file_name = i.next();
    if(!QFile::exists(file_name))
    {
      i.remove();
    }
  }

  for(int j = 0; j < max_recent_files; ++j)
  {
    if(j < m_sl_recent_files.count())
    {
      /////////////////////////////////////////////////////////////////////////////////////////////////////
      //display full name of OpenDAP URL or just last component of local file
      /////////////////////////////////////////////////////////////////////////////////////////////////////

      QString file_name;

      file_name = last_component(m_sl_recent_files[j]);
      QString text = tr("&%1 %2")
        .arg(j + 1)
        .arg(file_name);
      m_action_recent_file[j]->setText(text);
      m_action_recent_file[j]->setData(m_sl_recent_files[j]);
      m_action_recent_file[j]->setVisible(true);
    }
    else
    {
      m_action_recent_file[j]->setVisible(false);
    }
  }
  m_action_separator_recent->setVisible(!m_sl_recent_files.isEmpty());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//MainWindow::set_current_file
/////////////////////////////////////////////////////////////////////////////////////////////////////

void MainWindow::set_current_file(const QString &file_name)
{
  m_str_current_file = file_name;

  QString shownName = tr("Untitled");
  if(!m_str_current_file.isEmpty())
  {
    shownName = last_component(m_str_current_file);
    m_sl_recent_files.removeAll(m_str_current_file);
    m_sl_recent_files.prepend(m_str_current_file);
    update_recent_file_actions();
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//MainWindow::open_file
/////////////////////////////////////////////////////////////////////////////////////////////////////

void MainWindow::open_file()
{
  QString file_name = QFileDialog::getOpenFileName(this,
    tr("Open File"), ".",
    tr("HDF Files (*.h5);;All files (*.*)"));

  if(file_name.isEmpty())
    return;

  if(this->read_file(file_name) == 0)
  {
    this->set_current_file(file_name);
  }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////
//MainWindow::open_recent_file
/////////////////////////////////////////////////////////////////////////////////////////////////////

void MainWindow::open_recent_file()
{
  QAction *action = qobject_cast<QAction *>(sender());
  if(action)
  {
    read_file(action->data().toString());
  }
}

///////////////////////////////////////////////////////////////////////////////////////
//MainWindow::read_file
///////////////////////////////////////////////////////////////////////////////////////

int MainWindow::read_file(QString file_name)
{
  QByteArray ba;
  std::string path;
  std::string str_file_name;
  QString name;
  int index;
  int len;
  hid_t fid;

  //convert QString to char*
  ba = file_name.toLatin1();

  //convert to std::string
  str_file_name = ba.data();

  ///////////////////////////////////////////////////////////////////////////////////////
  //construct list of objects
  ///////////////////////////////////////////////////////////////////////////////////////

  m_visit.visit(str_file_name.c_str());

  ///////////////////////////////////////////////////////////////////////////////////////
  //populate objects
  ///////////////////////////////////////////////////////////////////////////////////////

  if((fid = H5Fopen(str_file_name.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT)) < 0)
  {
    return -1;
  }

  //item data group item
  ItemData *item_data_grp = new ItemData(ItemData::Group, str_file_name, "/", (hdf_dataset_t*)NULL);

  //add root
  QTreeWidgetItem *root_item = new QTreeWidgetItem(m_tree);
  index = file_name.lastIndexOf(QChar('/'));
  len = file_name.length();
  name = file_name.right(len - index - 1);
  root_item->setText(0, name);
  root_item->setIcon(0, m_icon_group);
  //add data
  QVariant data;
  data.setValue(item_data_grp);
  root_item->setData(0, Qt::UserRole, data);

  if(iterate(str_file_name, "/", fid, root_item) < 0)
  {

  }

  if(H5Fclose(fid) < 0)
  {

  }

  return 0;
}


///////////////////////////////////////////////////////////////////////////////////////
//count_objects_cb
//callback function function for H5Literate
///////////////////////////////////////////////////////////////////////////////////////

static herr_t count_objects_cb(hid_t, const char *, const H5L_info_t *, void *_op_data)
{
  hsize_t *op_data = (hsize_t *)_op_data;

  (*op_data)++;

  return(H5_ITER_CONT);
}

///////////////////////////////////////////////////////////////////////////////////////
//get_name_type_cb
//callback function function for H5Literate
//////////////////////////////////////////////////////////////////////////////////////

static herr_t get_name_type_cb(hid_t loc_id, const char *name, const H5L_info_t *, void *op_data)
{
  H5G_stat_t stat_buf;

  if(H5Gget_objinfo(loc_id, name, 0, &stat_buf) < 0)
  {

  }

  ((name_type_t *)op_data)->type = stat_buf.type;
  ((name_type_t *)op_data)->name = (char *)strdup(name);

  // define H5_ITER_STOP for return. This will cause the iterator to stop 
  return H5_ITER_STOP;
}

///////////////////////////////////////////////////////////////////////////////////////
//MainWindow::find_object
//////////////////////////////////////////////////////////////////////////////////////

H5O_info_added_t* MainWindow::find_object(haddr_t addr)
{
  for(size_t idx = 0; idx < m_visit.visit_info.size(); idx++)
  {
    if(addr == m_visit.visit_info[idx].oinfo.addr)
    {
      return &(m_visit.visit_info[idx]);
    }
  }
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////
//MainWindow::iterate
//iterates in group specified by location id 'loc_id'
//full group name is used to recursevely construct a dataset path
//QTreeWidgetItem * is used to build the tree item hierarchy 
//passing new QTreeWidgetItem for group as parent
//for datasets, sizes and metadata are stored
///////////////////////////////////////////////////////////////////////////////////////

int MainWindow::iterate(const std::string& file_name, const std::string& grp_path, const hid_t loc_id, QTreeWidgetItem *tree_item_parent)
{
  hsize_t nbr_objects = 0;
  QTreeWidgetItem *item_grp = NULL;
  QTreeWidgetItem *item_var = NULL;
  hsize_t index;
  name_type_t info;
  ItemData *item_data;
  QVariant data;
  std::string path;
  bool do_iterate;
  size_t datatype_size;
  H5T_sign_t datatype_sign;
  H5T_class_t datatype_class;
  hid_t gid;
  hid_t did;
  hid_t sid;
  hid_t ftid;
  hid_t mtid;
  hsize_t dims[H5S_MAX_RANK];
  int rank;

  if(H5Literate(loc_id, H5_INDEX_NAME, H5_ITER_INC, NULL, count_objects_cb, &nbr_objects) < 0)
  {

  }

  for(hsize_t idx_obj = 0; idx_obj < nbr_objects; idx_obj++)
  {

    index = idx_obj;

    //'index' allows an interrupted iteration to be resumed; it is passed in by the application with a starting point 
    //and returned by the library with the point at which the iteration stopped

    if(H5Literate(loc_id, H5_INDEX_NAME, H5_ITER_INC, &index, get_name_type_cb, &info) < 0)
    {

    }

    // initialize path 
    path = grp_path;
    if(grp_path != "/")
      path += "/";
    path += info.name;

    switch(info.type)
    {
      ///////////////////////////////////////////////////////////////////////////////////////
      //H5G_GROUP
      //////////////////////////////////////////////////////////////////////////////////////

    case H5G_GROUP:

      if((gid = H5Gopen2(loc_id, info.name, H5P_DEFAULT)) < 0)
      {

      }

      H5O_info_t oinfo_buf;
      do_iterate = true;

      //get object info
      if(H5Oget_info(gid, &oinfo_buf) < 0)
      {

      }

      //group item
      item_grp = new QTreeWidgetItem(tree_item_parent);
      item_grp->setText(0, info.name);
      item_grp->setIcon(0, m_icon_group);
      //item data
      item_data = new ItemData(ItemData::Group, file_name, info.name, (hdf_dataset_t*)NULL);
      data.setValue(item_data);
      item_grp->setData(0, Qt::UserRole, data);

      if(oinfo_buf.rc > 1)
      {
        H5O_info_added_t *oinfo_added = find_object(oinfo_buf.addr);

        if(oinfo_added->added > 0)
        {
          //avoid infinite recursion due to a circular path in the file.
          do_iterate = false;
        }
        else
        {
          oinfo_added->added++;
        }
      }

      //iterate in sub group passing QTreeWidgetItem for group as parent
      if(do_iterate && iterate(file_name, path, gid, item_grp) < 0)
      {

      }

      if(get_attributes(file_name, path, gid, item_grp) < 0)
      {

      }

      if(H5Gclose(gid) < 0)
      {

      }

      free(info.name);
      break;

      ///////////////////////////////////////////////////////////////////////////////////////
      //H5G_DATASET
      //////////////////////////////////////////////////////////////////////////////////////

    case H5G_DATASET:

      if((did = H5Dopen2(loc_id, info.name, H5P_DEFAULT)) < 0)
      {

      }

      if((sid = H5Dget_space(did)) < 0)
      {

      }

      if((rank = H5Sget_simple_extent_dims(sid, dims, NULL)) < 0)
      {

      }

      if((ftid = H5Dget_type(did)) < 0)
      {

      }

      if((mtid = H5Tget_native_type(ftid, H5T_DIR_DEFAULT)) < 0)
      {

      }

      ///////////////////////////////////////////////////////////////////////////////////////
      //store datatype sizes and metadata needed to display HDF5 buffer data
      ///////////////////////////////////////////////////////////////////////////////////////

      if((datatype_size = H5Tget_size(mtid)) == 0)
      {

      }

      if((datatype_sign = H5Tget_sign(mtid)) < 0)
      {

      }

      if((datatype_class = H5Tget_class(mtid)) < 0)
      {

      }

      if(H5Sclose(sid) < 0)
      {

      }

      if(H5Tclose(ftid) < 0)
      {

      }

      if(H5Tclose(mtid) < 0)
      {

      }

      //store dimensions in ItemData
      std::vector< hsize_t> dim; //dataset dimensions
      for(int idx = 0; idx < rank; ++idx)
      {
        dim.push_back(dims[idx]);
      }

      //store a hdf_dataset_t with full path, dimensions and metadata
      hdf_dataset_t *dataset = new hdf_dataset_t(
        path.c_str(),
        dim,
        datatype_size,
        datatype_sign,
        datatype_class);

      //append item
      item_var = new QTreeWidgetItem(tree_item_parent);
      item_var->setText(0, info.name);
      item_var->setIcon(0, m_icon_dataset);
      //item data
      item_data = new ItemData(ItemData::Variable, file_name, info.name, dataset);
      data.setValue(item_data);
      item_var->setData(0, Qt::UserRole, data);

      if(get_attributes(file_name, path, did, item_var) < 0)
      {

      }

      if(H5Dclose(did) < 0)
      {

      }

      break;
    }

  }

  return 0;
}

///////////////////////////////////////////////////////////////////////////////////////
//MainWindow::get_attributes
// it is assumed that loc_id is either from 
// loc_id = H5Gopen( fid, name);
// loc_id = H5Dopen( fid, name);
// loc_id = H5Topen( fid, name);
///////////////////////////////////////////////////////////////////////////////////////

int MainWindow::get_attributes(const std::string& file_name, const std::string& path, const hid_t loc_id, QTreeWidgetItem *tree_item_parent)
{
  H5O_info_t oinfo;
  hid_t aid;
  size_t datatype_size;
  H5T_sign_t datatype_sign;
  H5T_class_t datatype_class;
  hid_t sid;
  hid_t ftid;
  hid_t mtid;
  hsize_t dims[H5S_MAX_RANK];
  char name[1024];
  int rank;
  ItemData *item_data;
  QVariant data;
  QTreeWidgetItem *item = NULL;

  //get object info
  if(H5Oget_info(loc_id, &oinfo) < 0)
  {

  }

  for(hsize_t idx = 0; idx < oinfo.num_attrs; idx++)
  {
    if((aid = H5Aopen_by_idx(loc_id, ".", H5_INDEX_CRT_ORDER, H5_ITER_INC, idx, H5P_DEFAULT, H5P_DEFAULT)) < 0)
    {

    }

    if(H5Aget_name(aid, (size_t)1024, name) < 0)
    {

    }

    if((sid = H5Aget_space(aid)) < 0)
    {

    }

    if((rank = H5Sget_simple_extent_dims(sid, dims, NULL)) < 0)
    {

    }

    if((ftid = H5Aget_type(aid)) < 0)
    {

    }

    if((mtid = H5Tget_native_type(ftid, H5T_DIR_DEFAULT)) < 0)
    {

    }

    ///////////////////////////////////////////////////////////////////////////////////////
    //store datatype sizes and metadata needed to display HDF5 buffer data
    ///////////////////////////////////////////////////////////////////////////////////////

    if((datatype_size = H5Tget_size(mtid)) == 0)
    {

    }

    if((datatype_sign = H5Tget_sign(mtid)) < 0)
    {

    }

    if((datatype_class = H5Tget_class(mtid)) < 0)
    {

    }

    if(H5Sclose(sid) < 0)
    {

    }

    if(H5Tclose(ftid) < 0)
    {

    }

    if(H5Tclose(mtid) < 0)
    {

    }

    if(H5Aclose(aid) < 0)
    {

    }

    //store dimensions in ItemData
    std::vector< hsize_t> dim; //dataset dimensions
    for(int idx = 0; idx < rank; ++idx)
    {
      dim.push_back(dims[idx]);
    }

    //store a hdf_dataset_t with full path, dimensions and metadata
    hdf_dataset_t *dataset = new hdf_dataset_t(
      path.c_str(),
      dim,
      datatype_size,
      datatype_sign,
      datatype_class);

    //append item
    item = new QTreeWidgetItem(tree_item_parent);
    item->setText(0, name);
    item->setIcon(0, m_icon_attribute);
    //item data
    item_data = new ItemData(ItemData::Attribute, file_name, name, dataset);
    data.setValue(item_data);
    item->setData(0, Qt::UserRole, data);
  }

  return 0;
}

///////////////////////////////////////////////////////////////////////////////////////
//FileTreeWidget::FileTreeWidget 
///////////////////////////////////////////////////////////////////////////////////////

FileTreeWidget::FileTreeWidget(QWidget *parent) : QTreeWidget(parent)
{
  setContextMenuPolicy(Qt::CustomContextMenu);

  //right click menu
  connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(show_context_menu(const QPoint &)));

  //double click
  connect(this, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(add_grid()));
}

///////////////////////////////////////////////////////////////////////////////////////
//FileTreeWidget::~FileTreeWidget
///////////////////////////////////////////////////////////////////////////////////////

FileTreeWidget::~FileTreeWidget()
{
  QTreeWidgetItemIterator it(this);
  while(*it)
  {
    ItemData *item_data = get_item_data(*it);
    delete item_data;
    ++it;
  }

}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//FileTreeWidget::load_item
/////////////////////////////////////////////////////////////////////////////////////////////////////

void FileTreeWidget::load_item(QTreeWidgetItem  *item)
{
  hid_t fid;
  hid_t did;
  hid_t sid;
  hid_t ftid;
  hid_t mtid;
  int rank;
  size_t datatype_size;
  H5T_sign_t datatype_sign;
  H5T_class_t datatype_class;
  hsize_t dims[H5S_MAX_RANK];
  hsize_t nbr_elements = 1;

  ItemData *item_data = get_item_data(item);
  assert(item_data->m_kind == ItemData::Variable);

  //if not loaded, read buffer from file 
  if(item_data->m_dataset->m_buf != NULL)
  {
    return;
  }

  if((fid = H5Fopen(item_data->m_file_name.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT)) < 0)
  {

  }

  if((did = H5Dopen2(fid, item_data->m_dataset->m_path.c_str(), H5P_DEFAULT)) < 0)
  {

  }

  if((sid = H5Dget_space(did)) < 0)
  {

  }

  if((rank = H5Sget_simple_extent_dims(sid, dims, NULL)) < 0)
  {

  }

  if((ftid = H5Dget_type(did)) < 0)
  {

  }

  if((mtid = H5Tget_native_type(ftid, H5T_DIR_DEFAULT)) < 0)
  {

  }

  if((datatype_size = H5Tget_size(mtid)) == 0)
  {

  }

  if((datatype_sign = H5Tget_sign(mtid)) < 0)
  {

  }

  if((datatype_class = H5Tget_class(mtid)) < 0)
  {

  }

  for(int idx = 0; idx < rank; idx++)
  {
    nbr_elements *= dims[idx];
  }

  item_data->m_dataset->m_buf = malloc(static_cast<size_t>(datatype_size * nbr_elements));

  if(H5Dread(did, mtid, H5S_ALL, H5S_ALL, H5P_DEFAULT, item_data->m_dataset->m_buf) < 0)
  {
    qDebug() << item_data->m_dataset->m_path.c_str();
  }

  if(H5Dclose(did) < 0)
  {

  }

  if(H5Sclose(sid) < 0)
  {

  }

  if(H5Tclose(ftid) < 0)
  {

  }

  if(H5Tclose(mtid) < 0)
  {

  }

  if(H5Fclose(fid) < 0)
  {

  }

}


/////////////////////////////////////////////////////////////////////////////////////////////////////
//FileTreeWidget::load_item_attribute
/////////////////////////////////////////////////////////////////////////////////////////////////////

void FileTreeWidget::load_item_attribute(QTreeWidgetItem  *item)
{
  hid_t fid;
  hid_t aid;
  hid_t sid;
  hid_t ftid;
  hid_t mtid;
  hid_t obj_id = -1;
  int rank;
  size_t datatype_size;
  H5T_sign_t datatype_sign;
  H5T_class_t datatype_class;
  hsize_t dims[H5S_MAX_RANK];
  hsize_t nbr_elements = 1;
  H5O_info_t oinfo;
  ItemData *item_data = get_item_data(item);
  assert(item_data->m_kind == ItemData::Attribute);
  const char* path = item_data->m_dataset->m_path.c_str();
  const char* name = item_data->m_item_nm.c_str();

  //if not loaded, read buffer from file 
  if(item_data->m_dataset->m_buf != NULL)
  {
    return;
  }

  if((fid = H5Fopen(item_data->m_file_name.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT)) < 0)
  {

  }

  //get object info
  if(H5Oget_info_by_name(fid, path, &oinfo, H5P_DEFAULT) < 0)
  {

  }

  // open the PARENT object 
  switch(oinfo.type)
  {
  case H5G_DATASET:
    if((obj_id = H5Dopen2(fid, path, H5P_DEFAULT)) < 0)
    {
    }
    break;
  case H5G_GROUP:
    if((obj_id = H5Gopen2(fid, path, H5P_DEFAULT)) < 0)
    {

    }
    break;
  case H5G_TYPE:
    if((obj_id = H5Topen2(fid, path, H5P_DEFAULT)) < 0)
    {

    }
    break;
  default:
    assert(0);
    break;
  }

  if((aid = H5Aopen(obj_id, name, H5P_DEFAULT)) < 0)
  {

  }

  if((sid = H5Aget_space(aid)) < 0)
  {

  }

  if((rank = H5Sget_simple_extent_dims(sid, dims, NULL)) < 0)
  {

  }

  if((ftid = H5Aget_type(aid)) < 0)
  {

  }

  if((mtid = H5Tget_native_type(ftid, H5T_DIR_DEFAULT)) < 0)
  {

  }

  if((datatype_size = H5Tget_size(mtid)) == 0)
  {

  }

  if((datatype_sign = H5Tget_sign(mtid)) < 0)
  {

  }

  if((datatype_class = H5Tget_class(mtid)) < 0)
  {

  }

  for(int idx = 0; idx < rank; idx++)
  {
    nbr_elements *= dims[idx];
  }

  item_data->m_dataset->m_buf = malloc(static_cast<size_t>(datatype_size * nbr_elements));

  if(H5Aread(aid, mtid, item_data->m_dataset->m_buf) < 0)
  {
    qDebug() << item_data->m_dataset->m_path.c_str() << ":" << item_data->m_item_nm.c_str();
  }

  if(H5Aclose(aid) < 0)
  {

  }

  if(H5Sclose(sid) < 0)
  {

  }

  if(H5Tclose(ftid) < 0)
  {

  }

  if(H5Tclose(mtid) < 0)
  {

  }

  // close the PARENT object 
  switch(oinfo.type)
  {
  case H5G_DATASET:
    if(H5Dclose(obj_id) < 0)
    {
    };
    break;
  case H5G_GROUP:
    if(H5Gclose(obj_id) < 0)
    {
    };
    break;
  case H5G_TYPE:
    if(H5Tclose(obj_id) < 0)
    {
    };
    break;
  default:
    assert(0);
    break;
  }

  if(H5Fclose(fid) < 0)
  {

  }


}

///////////////////////////////////////////////////////////////////////////////////////
//FileTreeWidget::show_context_menu
///////////////////////////////////////////////////////////////////////////////////////

void FileTreeWidget::show_context_menu(const QPoint &p)
{
  QTreeWidgetItem *item = static_cast <QTreeWidgetItem*> (itemAt(p));
  ItemData *item_data = get_item_data(item);
  if(item_data->m_kind == ItemData::Group)
  {
    return;
  }
  QMenu menu;
  QAction *action_grid = new QAction("Grid...", this);
  if(item_data->m_dataset->m_datatype_class != H5T_INTEGER &&
    item_data->m_dataset->m_datatype_class != H5T_FLOAT)
  {
    action_grid->setEnabled(false);
  }
  connect(action_grid, SIGNAL(triggered()), this, SLOT(add_grid()));
  menu.addAction(action_grid);
  menu.exec(QCursor::pos());
}

///////////////////////////////////////////////////////////////////////////////////////
//FileTreeWidget::add_grid
///////////////////////////////////////////////////////////////////////////////////////

void FileTreeWidget::add_grid()
{
  QTreeWidgetItem *item = static_cast <QTreeWidgetItem*> (currentItem());
  ItemData *item_data = get_item_data(item);
  assert(item_data->m_kind == ItemData::Variable || item_data->m_kind == ItemData::Attribute);
  if(item_data->m_dataset->m_datatype_class != H5T_INTEGER &&
    item_data->m_dataset->m_datatype_class != H5T_FLOAT)
  {
    return;
  }
  if(item_data->m_kind == ItemData::Variable)
  {
    this->load_item(item);
  }
  else if(item_data->m_kind == ItemData::Attribute)
  {
    this->load_item_attribute(item);
  }
  m_main_window->add_table(item_data);

}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//TableModel
/////////////////////////////////////////////////////////////////////////////////////////////////////

class TableModel : public QAbstractTableModel
{
public:
  TableModel(QObject *parent, ItemData *item_data);
  int rowCount(const QModelIndex &parent = QModelIndex()) const;
  int columnCount(const QModelIndex &parent = QModelIndex()) const;
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

  hdf_dataset_t *m_dataset; // HDF variable to display (convenience pointer to data in ItemData) 
  ChildWindow* m_widget; //get layers in toolbar
  int m_nbr_rows;   // number of rows
  int m_nbr_cols;   // number of columns
  void data_changed(); //update table view when change of layer
private:
  ItemData *m_item_data; // the tree item that generated this grid 
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
//TableModel::TableModel
/////////////////////////////////////////////////////////////////////////////////////////////////////

TableModel::TableModel(QObject *parent, ItemData *item_data) :
QAbstractTableModel(parent),
m_dataset(item_data->m_dataset),
m_widget(NULL),
m_item_data(item_data)
{
  assert(m_dataset->m_dim.size() <= H5S_MAX_RANK);

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  //define grid
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  if(m_dataset->m_dim.size() == 0)
  {
    m_nbr_rows = 1;
    m_nbr_cols = 1;
  }
  else if(m_dataset->m_dim.size() == 1)
  {
    m_nbr_rows = m_dataset->m_dim[0];
    m_nbr_cols = 1;
  }
  else
  {
    m_nbr_rows = m_dataset->m_dim[m_dataset->m_dim.size() - 2]; //index 1 for 3D
    m_nbr_cols = m_dataset->m_dim[m_dataset->m_dim.size() - 1]; //index 2 for 3D
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//TableModel::rowCount
/////////////////////////////////////////////////////////////////////////////////////////////////////

int TableModel::rowCount(const QModelIndex &) const
{
  return m_nbr_rows;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//TableModel::columnCount
/////////////////////////////////////////////////////////////////////////////////////////////////////

int TableModel::columnCount(const QModelIndex &) const
{
  return m_nbr_cols;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//TableModel::data_changed
//update table view when change of layer
/////////////////////////////////////////////////////////////////////////////////////////////////////

void TableModel::data_changed()
{
  QModelIndex top = index(0, 0, QModelIndex());
  QModelIndex bottom = index(m_nbr_rows, m_nbr_cols, QModelIndex());
  dataChanged(top, bottom);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//TableModel::data
/////////////////////////////////////////////////////////////////////////////////////////////////////

QVariant TableModel::data(const QModelIndex &index, int role) const
{
  ChildWindow* parent = m_widget;
  QString str;
  size_t idx_buf = 0;

  //start of offset
  //3D
  if(parent->m_layer.size() == 1)
  {
    idx_buf = parent->m_layer[0] * m_nbr_rows * m_nbr_cols;
  }
  //4D
  else if(parent->m_layer.size() == 2)
  {
    idx_buf = parent->m_layer[0] * m_dataset->m_dim[1] + parent->m_layer[1];
    idx_buf *= m_nbr_rows * m_nbr_cols;
  }
  //5D
  else if(parent->m_layer.size() == 3)
  {
    idx_buf = parent->m_layer[0] * m_dataset->m_dim[1] * m_dataset->m_dim[2]
      + parent->m_layer[1] * m_dataset->m_dim[2]
      + parent->m_layer[2];
    idx_buf *= m_nbr_rows * m_nbr_cols;
  }

  //into current index
  idx_buf += index.row() * m_nbr_cols + index.column();

  if(role != Qt::DisplayRole)
  {
    return QVariant();
  }

  switch(m_dataset->m_datatype_class)
  {
    ///////////////////////////////////////////////////////////////////////////////////////
    //H5T_STRING
    ///////////////////////////////////////////////////////////////////////////////////////

  case H5T_STRING:

    break;

    ///////////////////////////////////////////////////////////////////////////////////////
    //H5T_FLOAT
    ///////////////////////////////////////////////////////////////////////////////////////

  case H5T_FLOAT:
    if(sizeof(float) == m_dataset->m_datatype_size)
    {
      float *buf_ = static_cast<float*> (m_dataset->m_buf);
      str.sprintf("%g", buf_[idx_buf]);
      return str;
    }
    else if(sizeof(double) == m_dataset->m_datatype_size)
    {
      double *buf_ = static_cast<double*> (m_dataset->m_buf);
      str.sprintf("%g", buf_[idx_buf]);
      return str;
    }
#if H5_SIZEOF_LONG_DOUBLE !=0
    else if(sizeof(long double) == m_dataset->m_datatype_size)
    {
      long double *buf_;
      buf_ = static_cast<long double*> (m_dataset->m_buf);
      str.sprintf("%Lf", buf_[idx_buf]);
      return str;
    }
#endif
    break;

  case H5T_INTEGER:

    ///////////////////////////////////////////////////////////////////////////////////////
    //H5T_INTEGER
    ///////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////
    //H5T_NATIVE_SCHAR H5T_NATIVE_UCHAR
    ///////////////////////////////////////////////////////////////////////////////////////

    if(sizeof(char) == m_dataset->m_datatype_size)
    {
      if(H5T_SGN_NONE == m_dataset->m_datatype_sign)
      {
        unsigned char *buf_ = static_cast<unsigned char*> (m_dataset->m_buf);
        str.sprintf("%u", buf_[idx_buf]);
        return str;
      }
      else
      {
        signed char *buf_ = static_cast<signed char*> (m_dataset->m_buf);
        str.sprintf("%hhd", buf_[idx_buf]);
        return str;
      }
    }

    ///////////////////////////////////////////////////////////////////////////////////////
    //H5T_NATIVE_SHORT H5T_NATIVE_USHORT
    ///////////////////////////////////////////////////////////////////////////////////////

    else if(sizeof(short) == m_dataset->m_datatype_size)
    {
      if(H5T_SGN_NONE == m_dataset->m_datatype_sign)
      {
        unsigned short *buf_ = static_cast<unsigned short*> (m_dataset->m_buf);
        str.sprintf("%u", buf_[idx_buf]);
        return str;

      }
      else
      {
        short *buf_ = static_cast<short*> (m_dataset->m_buf);
        str.sprintf("%d", buf_[idx_buf]);
        return str;

      }

    }

    ///////////////////////////////////////////////////////////////////////////////////////
    //H5T_NATIVE_INT H5T_NATIVE_UINT
    ///////////////////////////////////////////////////////////////////////////////////////

    else if(sizeof(int) == m_dataset->m_datatype_size)
    {
      if(H5T_SGN_NONE == m_dataset->m_datatype_sign)
      {
        unsigned int* buf_ = static_cast<unsigned int*> (m_dataset->m_buf);
        str.sprintf("%u", buf_[idx_buf]);
        return str;

      }
      else
      {
        int* buf_ = static_cast<int*> (m_dataset->m_buf);
        str.sprintf("%d", buf_[idx_buf]);
        return str;
      }
    }

    ///////////////////////////////////////////////////////////////////////////////////////
    //H5T_NATIVE_LONG H5T_NATIVE_ULONG
    ///////////////////////////////////////////////////////////////////////////////////////

    else if(sizeof(long) == m_dataset->m_datatype_size)
    {

      if(H5T_SGN_NONE == m_dataset->m_datatype_sign)
      {
        unsigned long* buf_ = static_cast<unsigned long*> (m_dataset->m_buf);
        str.sprintf("%lu", buf_[idx_buf]);
        return str;

      }
      else
      {
        long* buf_ = static_cast<long*> (m_dataset->m_buf);
        str.sprintf("%ld", buf_[idx_buf]);
        return str;

      }

    }

    ///////////////////////////////////////////////////////////////////////////////////////
    //H5T_NATIVE_LLONG H5T_NATIVE_ULLONG
    ///////////////////////////////////////////////////////////////////////////////////////

    else if(sizeof(long long) == m_dataset->m_datatype_size)
    {

      if(H5T_SGN_NONE == m_dataset->m_datatype_sign)
      {
        unsigned long long* buf_ = static_cast<unsigned long long*> (m_dataset->m_buf);
        str.sprintf("%llu", buf_[idx_buf]);
        return str;

      }
      else
      {
        long long* buf_ = static_cast<long long*> (m_dataset->m_buf);
        str.sprintf("%lld", buf_[idx_buf]);
        return str;

      }

    }

    break;

    ///////////////////////////////////////////////////////////////////////////////////////
    //H5T_COMPOUND
    ///////////////////////////////////////////////////////////////////////////////////////

  case H5T_COMPOUND:

    break;


    ///////////////////////////////////////////////////////////////////////////////////////
    //H5T_ENUM
    ///////////////////////////////////////////////////////////////////////////////////////

  case H5T_ENUM:

    break;

    ///////////////////////////////////////////////////////////////////////////////////////
    //H5T_ARRAY
    ///////////////////////////////////////////////////////////////////////////////////////

  case H5T_ARRAY:

    break;

    ///////////////////////////////////////////////////////////////////////////////////////
    //H5T_VLEN
    ///////////////////////////////////////////////////////////////////////////////////////

  case H5T_VLEN:

    break;

    ///////////////////////////////////////////////////////////////////////////////////////
    //H5T_TIME H5T_BITFIELD H5T_OPAQUE
    ///////////////////////////////////////////////////////////////////////////////////////

  case H5T_TIME:
  case H5T_BITFIELD:
  case H5T_OPAQUE:

    break;

    ///////////////////////////////////////////////////////////////////////////////////////
    //H5T_REFERENCE
    ///////////////////////////////////////////////////////////////////////////////////////

  case H5T_REFERENCE:


    break;

  default:


    break;

  }; //switch

}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//ChildWindowTable
//model/view
/////////////////////////////////////////////////////////////////////////////////////////////////////

class ChildWindowTable : public ChildWindow
{
public:
  ChildWindowTable(QWidget *parent, ItemData *item_data) :
    ChildWindow(parent, item_data)
  {
    //each new table widget has its own model
    m_model = new TableModel(this, item_data);
    m_model->m_widget = this;
    m_table = new QTableView(this);
    m_table->setModel(m_model);

    //set default row height
    QHeaderView *verticalHeader = m_table->verticalHeader();
#if QT_VERSION >= 0x050000
    verticalHeader->sectionResizeMode(QHeaderView::Fixed);
#else
    verticalHeader->setResizeMode(QHeaderView::Fixed);
#endif
    verticalHeader->setDefaultSectionSize(24);
    setCentralWidget(m_table);
  }
private:
  QTableView *m_table;
};


///////////////////////////////////////////////////////////////////////////////////////
//MainWindow::add_table
///////////////////////////////////////////////////////////////////////////////////////

void MainWindow::add_table(ItemData *item_data)
{
  ChildWindowTable *window = new ChildWindowTable(this, item_data);
  m_mdi_area->addSubWindow(window);
  window->show();
}

///////////////////////////////////////////////////////////////////////////////////////
//MainWindow::add_image
///////////////////////////////////////////////////////////////////////////////////////

void MainWindow::add_image(ItemData *)
{

}

///////////////////////////////////////////////////////////////////////////////////////
//ChildWindow::ChildWindow
///////////////////////////////////////////////////////////////////////////////////////

ChildWindow::ChildWindow(QWidget *parent, ItemData *item_data) :
QMainWindow(parent),
m_dataset(item_data->m_dataset)
{
  QString str;
  str.sprintf(" : %s", item_data->m_item_nm.c_str());
  this->setWindowTitle(last_component(item_data->m_file_name.c_str()) + str);

  //currently selected layers for dimensions greater than two are the first layer
  if(m_dataset->m_dim.size() > 2)
  {
    for(size_t idx_dmn = 0; idx_dmn < m_dataset->m_dim.size() - 2; idx_dmn++)
    {
      m_layer.push_back(0);
    }
  }

  QSignalMapper *signal_mapper_next = NULL;
  QSignalMapper *signal_mapper_previous = NULL;
  QSignalMapper *signal_mapper_combo = NULL;

  //data has layers
  if(m_dataset->m_dim.size() > 2)
  {
    m_tool_bar = addToolBar(tr("Layers"));

    signal_mapper_next = new QSignalMapper(this);
    signal_mapper_previous = new QSignalMapper(this);
    signal_mapper_combo = new QSignalMapper(this);
    connect(signal_mapper_next, SIGNAL(mapped(int)), this, SLOT(next_layer(int)));
    connect(signal_mapper_previous, SIGNAL(mapped(int)), this, SLOT(previous_layer(int)));
    connect(signal_mapper_combo, SIGNAL(mapped(int)), this, SLOT(combo_layer(int)));
  }

  //number of dimensions above a two-dimensional dataset
  for(size_t idx_dmn = 0; idx_dmn < m_layer.size(); idx_dmn++)
  {
    ///////////////////////////////////////////////////////////////////////////////////////
    //next layer
    ///////////////////////////////////////////////////////////////////////////////////////

    QAction *action_next  = new QAction(tr("&Next layer..."), this);
    action_next->setIcon(QIcon(":/images/right.png"));
    action_next->setStatusTip(tr("Next layer"));
    connect(action_next, SIGNAL(triggered()), signal_mapper_next, SLOT(map()));
    signal_mapper_next->setMapping(action_next, idx_dmn);

    ///////////////////////////////////////////////////////////////////////////////////////
    //previous layer
    ///////////////////////////////////////////////////////////////////////////////////////

    QAction *action_previous = new QAction(tr("&Previous layer..."), this);
    action_previous->setIcon(QIcon(":/images/left.png"));
    action_previous->setStatusTip(tr("Previous layer"));
    connect(action_previous, SIGNAL(triggered()), signal_mapper_previous, SLOT(map()));
    signal_mapper_previous->setMapping(action_previous, idx_dmn);

    ///////////////////////////////////////////////////////////////////////////////////////
    //add to toolbar
    ///////////////////////////////////////////////////////////////////////////////////////

    m_tool_bar->addAction(action_next);
    m_tool_bar->addAction(action_previous);

    ///////////////////////////////////////////////////////////////////////////////////////
    //add combo box with layers, fill with possible coordinate variables and store combo in vector 
    ///////////////////////////////////////////////////////////////////////////////////////

    QComboBox *combo = new QComboBox;
    QFont font = combo->font();
    font.setPointSize(9);
    combo->setFont(font);
    QStringList list;

    for(unsigned int idx = 0; idx < m_dataset->m_dim[idx_dmn]; idx++)
    {
      str.sprintf("%u", idx + 1);
      list.append(str);
    }


    combo->addItems(list);
    connect(combo, SIGNAL(currentIndexChanged(int)), signal_mapper_combo, SLOT(map()));
    signal_mapper_combo->setMapping(combo, idx_dmn);
    m_tool_bar->addWidget(combo);
    m_vec_combo.push_back(combo);
  }


}

///////////////////////////////////////////////////////////////////////////////////////
//ChildWindow::previous_layer
///////////////////////////////////////////////////////////////////////////////////////

void ChildWindow::previous_layer(int idx_layer)
{
  m_layer[idx_layer]--;
  if(m_layer[idx_layer] < 0)
  {
    m_layer[idx_layer] = 0;
    return;
  }
  QComboBox *combo = m_vec_combo.at(idx_layer);
  combo->setCurrentIndex(m_layer[idx_layer]);
  m_model->data_changed();
  update();

}

///////////////////////////////////////////////////////////////////////////////////////
//ChildWindow::next_layer
///////////////////////////////////////////////////////////////////////////////////////

void ChildWindow::next_layer(int idx_layer)
{
  m_layer[idx_layer]++;
  if((size_t)m_layer[idx_layer] >= m_dataset->m_dim[idx_layer])
  {
    m_layer[idx_layer] = m_dataset->m_dim[idx_layer] - 1;
    return;
  }
  QComboBox *combo = m_vec_combo.at(idx_layer);
  combo->setCurrentIndex(m_layer[idx_layer]);
  m_model->data_changed();
  update();

}

///////////////////////////////////////////////////////////////////////////////////////
//ChildWindow::combo_layer
///////////////////////////////////////////////////////////////////////////////////////

void ChildWindow::combo_layer(int idx_layer)
{
  QComboBox *combo = m_vec_combo.at(idx_layer);
  m_layer[idx_layer] = combo->currentIndex();;
  m_model->data_changed();
  update();
}


