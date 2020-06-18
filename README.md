Tested under RGB-D_CAMERA_MAIN_V1.0 with micron lpddr4.(Based on imx8mq-evk board)

__YOUR_COMPILER_PATH__ : /home/ryu/workspace/gcc/gcc-7.4/usr/bin
__YOUR_COMPILER_PREFIX__ : aarch64-buildroot-linux-gnu-
__YOUR_BOARD_ARCHITECTURE__ : arm64
__YOUR_SOURCE_DIRECTORY__ : /home/ryu/workspace/uvc-gadget/rgbdsensor


1. How to compile source.
   1) untar gcc-7.4.tar.gz at /home/ryu/workspace/gcc/7.4 
      cd /home/ryu/workspace/gcc/7.4
      tar -xvf gcc-7.4.tar.gz
      
   2) if you have other path or compiler, modify tool.sh  (You must have appropriate compilers)
      cd __YOUR_SOURCE_DIRECTORY__
      vi tool.sh
      export PATH=__YOUR_COMPILER_PATH__
      export CROSS_COMPILE=__YOUR_COMPILER_PREFIX__
      export ARCH=__YOUR_BOARD_ARCHITECTURE__

   3) edit source tool.sh in terminal
      source tool.sh
      
   4) build source
      cd __YOUR_SOURCE_DIRECTORY__
      make
      * output :
       1) ./rgbd_uvc_main : RGBD uvc gadget application using capture data.(with class) 
       2) ./rgbd_uvc : RGBD uvc gadget application using capture data.
	   3) ./rgbd : RGBD sensor test application using capture data.
	   4) ./uvc : UVC gadget test application using dummy image. 
	   5) ./test : ksw's RGBD uvc gadget application using dummy image.
	   6) ./test1 : ksw's RGBD uvc gadget application using capture data.
      
   
2. How to execute application.
   * On your board (device)
   1) Copy rgbd_uvc_main, rgbd and uvc to your board. 


   * Plug in one of the USB cable to USB connector on your board.
     Plug in the other end of the USB cable to your other board or pc.
    
   * other board or pc  (host)
   1) ./capture : uvc capture test application.
      (if your pc is ubuntu, you can use cheese application.)
     
3. How to stop application.
   1) Ctrl+c
