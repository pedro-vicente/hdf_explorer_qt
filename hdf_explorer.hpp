#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#if QT_VERSION >= 0x050000
#include <QtWidgets>
#endif
#include <QtGui>
#include <QIcon>
#include <QMdiArea>
#include <string>
#include <vector>
#include "hdf5.h"
#include "visit.hpp"

class MainWindow;
class ItemData;
class hdf_dataset_t;
class TableModel;

/////////////////////////////////////////////////////////////////////////////////////////////////////
//FileTreeWidget
/////////////////////////////////////////////////////////////////////////////////////////////////////

class FileTreeWidget : public QTreeWidget
{
  Q_OBJECT
public:
  FileTreeWidget(QWidget *parent = 0);
  ~FileTreeWidget();
  private slots:
  void show_context_menu(const QPoint &);
  void add_grid();

public:
  void set_main_window(MainWindow *p)
  {
    m_main_window = p;
  }

private:
  MainWindow *m_main_window;
  void load_item(QTreeWidgetItem *);
  void load_item_attribute(QTreeWidgetItem *);
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
//MainWindow
/////////////////////////////////////////////////////////////////////////////////////////////////////

class MainWindow : public QMainWindow
{
  Q_OBJECT
public:
  MainWindow();
  void add_table(ItemData *item_data);
  void add_image(ItemData *item_data);
  int read_file(QString file_name);

  private slots:
  void open_recent_file();
  void open_file();
  void about();

private:

  ///////////////////////////////////////////////////////////////////////////////////////
  //widgets
  ///////////////////////////////////////////////////////////////////////////////////////

  QMenu *m_menu_file;
  QMenu *m_menu_help;
  QMenu *m_menu_windows;
  QToolBar *m_tool_bar;
  QMdiArea *m_mdi_area;
  FileTreeWidget *m_tree;
  QDockWidget *m_tree_dock;

  ///////////////////////////////////////////////////////////////////////////////////////
  //actions
  ///////////////////////////////////////////////////////////////////////////////////////

  QAction *m_action_open;
  QAction *m_action_exit;
  QAction *m_action_about;
  QAction *m_action_tile;
  QAction *m_action_close_all;

  ///////////////////////////////////////////////////////////////////////////////////////
  //icons
  ///////////////////////////////////////////////////////////////////////////////////////

  QIcon m_icon_main;
  QIcon m_icon_group;
  QIcon m_icon_dataset;
  QIcon m_icon_attribute;
  QIcon m_icon_image_indexed;
  QIcon m_icon_image_true;

  ///////////////////////////////////////////////////////////////////////////////////////
  //recent files
  ///////////////////////////////////////////////////////////////////////////////////////

  enum { max_recent_files = 5 };
  QAction *m_action_recent_file[max_recent_files];
  QAction *m_action_separator_recent;
  QStringList m_sl_recent_files;
  QString m_str_current_file;
  void update_recent_file_actions();
  void set_current_file(const QString &file_name);
  void closeEvent(QCloseEvent *eve);

  ///////////////////////////////////////////////////////////////////////////////////////
  //HDF5
  ///////////////////////////////////////////////////////////////////////////////////////

private:

  h5visit_t m_visit;

  int iterate(const std::string& file_name, const std::string& grp_name, const hid_t loc_id, QTreeWidgetItem *tree_item_parent);
  int get_attributes(const std::string& file_name, const std::string& grp_name, const hid_t loc_id, QTreeWidgetItem *tree_item_parent);

  //find object in list 
  H5O_info_added_t* find_object(haddr_t addr);
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
//ChildWindow
/////////////////////////////////////////////////////////////////////////////////////////////////////

class ChildWindow : public QMainWindow
{
  Q_OBJECT
public:
  ChildWindow(QWidget *parent, ItemData *item_data);
  std::vector<int> m_layer;  // current selected layer of a dimension > 2 

  private slots:
  void previous_layer(int);
  void next_layer(int);
  void combo_layer(int);

private:
  QToolBar *m_tool_bar;
  std::vector<QComboBox *> m_vec_combo;

protected:
  TableModel *m_model;
  hdf_dataset_t *m_dataset; // HDF variable to display (convenience pointer to data in ItemData)
};

#endif


