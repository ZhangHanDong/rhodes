package org.garret.perst.impl;
import net.rim.device.api.system.DeviceInfo;

import org.garret.perst.*;

import java.io.IOException;
import java.io.EOFException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Enumeration;
import javax.microedition.io.*;
import javax.microedition.io.file.*;
import java.util.Vector;

public class Jsr75File implements SimpleFile 
{
    private FileConnection fconn;
    private InputStream    in;
    private OutputStream   out;
    private long           inPos;
    private long           outPos;
    private long           currPos;
    private long           fileSize;
    private boolean        noFlush;

    private static final int ZERO_BUF_SIZE = 4096;
    private static Vector m_arRoots;
    private static String m_strRhoPath;
    
	static private void log(String txt) {
		System.out.println(txt);
	}

	static void createDir( String strDir  )throws IOException{
		log("Dir:"+strDir);
		FileConnection fdir = null;
		try{
			fdir = (FileConnection)Connector.open(strDir);//,Connector.READ_WRITE);
			log("Dir Connection opened.");
	        // If no exception is thrown, then the URI is valid, but the file may or may not exist.
	        if (!fdir.exists()) { 
	        	log("Create directory.");
	        	fdir.mkdir();  // create the dir if it doesn't exist
	        }else{
	        	log("Dir exists.");	        	
	        }
		}catch(IOException exc){
			log("Create directory failed." + exc.getMessage());
			throw exc;
		}finally{
			if ( fdir != null )
				fdir.close();
		}
	}

    static private Vector getRoots(){
    	if ( m_arRoots != null )
    		return m_arRoots;
    	
    	m_arRoots = new Vector();
    	Enumeration e = FileSystemRegistry.listRoots();
        while (e.hasMoreElements()) {
        	String strRoot = (String)e.nextElement();
        	log("Root: " + strRoot);
        	
        	m_arRoots.addElement(strRoot);
        }    	
        
        return m_arRoots;
    }
	
    static private String findRoot(String strRoot){
    	Vector arRoots = getRoots();
    	for( int i = 0; i < arRoots.size(); i++ ){
    		String strRoot1 = (String)arRoots.elementAt(i);
        	if ( strRoot1.equalsIgnoreCase(strRoot) ){
        		return strRoot1;
        	}
    	}
    	
    	return null;
    }
    
    static String makeRootPath(){
    	String strRoot = findRoot("SDCard/");
    	
    	if ( strRoot != null )
    		return strRoot;
    	
		String strVer = DeviceInfo.getPlatformVersion();
		if ( strVer == null || strVer.length() == 0 )//emulator
			strRoot = findRoot("store/");
		else
		{
    		int nDot = strVer.indexOf('.');
    		int nMajor = 0;
    		int nMinor = 0;
    		
    		if ( nDot >= 0 )
    		{
    			nMinor = Integer.parseInt( strVer.substring(nDot+1) );
    			nMajor = Integer.parseInt( strVer.substring(0, nDot) );
    		}else
    			nMajor = Integer.parseInt( strVer );
    		
    		if ( nMajor >= 4 && nMinor >= 3 ){
    			strRoot = findRoot("store/");	    		
    		}
		}
		
    	return strRoot;
    }
    
	//http://supportforums.blackberry.com/rim/board/message?board.id=java_dev&thread.id=6553            	
    static String getRhoPath() throws IOException{
    	if ( m_strRhoPath != null )
    		return m_strRhoPath;
    	
    	String strRoot = makeRootPath();
    	m_strRhoPath = "file:///" + strRoot;
    	
    	if ( strRoot.equalsIgnoreCase("store/") )
    		m_strRhoPath += "home/user/";
    	
    	m_strRhoPath += "Rho/";
    	createDir( m_strRhoPath );
    	
    	return m_strRhoPath;
    }
    
    public void open(String path, boolean readOnly, boolean noFlush) 
    {
        String url = path;
        this.noFlush = noFlush;
        log(path);
        if (!url.startsWith("file:")) { 
            if (url.startsWith("/")) { 
                url = "file:///" + path;
            } else { 
           
            	try{
	            	String strRhoPath = getRhoPath();
	            	url = strRhoPath + path;
            	} catch (IOException x) { 
                 	log("Exception: " + x.getMessage());
                     throw new StorageError(StorageError.FILE_ACCESS_ERROR, x);
                 }              	
            }
        }
        
        try { 
            if (fconn == null) {
                fconn = (FileConnection)Connector.open(url,Connector.READ_WRITE);
                // If no exception is thrown, then the URI is valid, but the file may or may not exist.
                if (!fconn.exists()) { 
                    fconn.create();  // create the file if it doesn't exist
                }
            }
            fileSize = fconn.fileSize();
            if (!readOnly) {
                //fconn.setWritable(true);	                    
            	
                out = fconn.openOutputStream();
                outPos = 0;
            }
        } catch (IOException x) { 
        	log("Exception: " + x.getMessage());
            throw new StorageError(StorageError.FILE_ACCESS_ERROR, x);
        }        
        in = null;
        inPos = 0;
    }

    public int read(long pos, byte[] b)
    {
        int len = b.length;

        if (pos >= fileSize) { 
            return 0;
        }

        try {
            if (in == null || inPos > pos) { 
                sync(); 
                if (in != null) { 
                    in.close();
                }
                in = fconn.openInputStream();
                inPos = in.skip(pos);
            } else if (inPos < pos) { 
                inPos += in.skip(pos - inPos);
            }
            if (inPos != pos) { 
                return 0;
            }        
            int off = 0;
            while (len > 0) { 
                int rc = in.read(b, off, len);
                if (rc > 0) { 
                    inPos += rc;
                    off += rc;
                    len -= rc;
                } else { 
                    break;
                }
            }
            return off;
        } catch(IOException x) { 
            throw new StorageError(StorageError.FILE_ACCESS_ERROR, x);
        } 
    }

    public void write(long pos, byte[] b)
    {
        int len = b.length;
        if (out == null) { 
            throw new StorageError(StorageError.FILE_ACCESS_ERROR, "Illegal mode");
        }
        try {
            if (outPos != pos) {                         
                out.close();
                out = fconn.openOutputStream(pos);
                if (pos > fileSize) { 
                    byte[] zeroBuf = new byte[ZERO_BUF_SIZE];
                    do { 
                        int size = pos - fileSize > ZERO_BUF_SIZE ? ZERO_BUF_SIZE : (int)(pos - fileSize);
                        out.write(zeroBuf, 0, size);
                        fileSize += size;
                        
                        //BB
                        fconn.truncate(fileSize);
                    } while (pos != fileSize);
                }
                outPos = pos;
            }
            out.write(b, 0, len);
            outPos += len;
            if (outPos > fileSize) { 
                fileSize = outPos;
            }
            //BB
            fconn.truncate(fileSize);
            if (in != null) { 
                in.close();
                in = null;
            }
        }catch(Exception exc){
        	throw new StorageError(StorageError.FILE_ACCESS_ERROR, exc);
        }
    }

    public long length() { 
        return fileSize;
    }

    public void close() { 
        try {
            if (in != null) { 
                in.close();
                in = null;
            }
            if (out != null) { 
                out.close();
                out = null;
            }
            fconn.close();
        } catch(IOException x) { 
            throw new StorageError(StorageError.FILE_ACCESS_ERROR, x);
        }
    }
    
    public void sync() {
        if (!noFlush && out != null) { 
            try { 
                out.flush();
            } catch(Exception x) { 
                throw new StorageError(StorageError.FILE_ACCESS_ERROR, x);
            }
        }
    }
}
