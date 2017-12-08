# hdf-explorer
HDF Explorer is a multi-platfrom graphical browser for HDF files. http://www.space-research.org/ 

HDF Explorer
====================

<img src="https://cloud.githubusercontent.com/assets/6119070/11098722/66e4ad1c-886c-11e5-9bd2-097b15457102.png">

HDF Explorer is a multi-platfrom graphical browser for HDF files.


Dependencies
------------

<img src="https://cloud.githubusercontent.com/assets/6119070/13334137/231ea0f8-dbd0-11e5-8546-8a409d80aa6d.png">

[Qt](http://www.qt.io/)
Qt is a cross-platform application framework for creating graphical user interfaces.
<br /> 

[HDF](https://www.hdfgroup.org/)
HDF is a set of software libraries and self-describing, 
machine-independent data formats that support the creation, 
access, and sharing of array-oriented scientific data.
<br /> 

Building from source
------------


Install dependency packages (Ubuntu):
<pre>
sudo apt-get install build-essential
sudo apt-get build-dep qt5-default
sudo apt-get install "^libxcb.*" libx11-xcb-dev libglu1-mesa-dev libxrender-dev libxi-dev
sudo apt-get install libgl1-mesa-dev
sudo apt-get install libhdf5-serial-dev
</pre>

Get source:
<pre>
git clone https://github.com/pedro-vicente/hdf_explorer_qt.git
</pre>

Build with:
<pre>
qmake
make
</pre>
