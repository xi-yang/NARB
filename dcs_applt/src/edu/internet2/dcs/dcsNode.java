/*
 * dcsNode.java
 *
 * Created on October 31, 2007, 10:40 AM
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package edu.internet2.dcs;

import java.awt.Color;
import java.awt.Font;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.Image;
import java.awt.MediaTracker;
import java.awt.Point;
import java.awt.RenderingHints;
import java.awt.Toolkit;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.awt.event.MouseMotionListener;
import java.awt.geom.Rectangle2D;
import java.awt.image.BufferedImage;
import java.net.URL;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Vector;
import java.util.prefs.Preferences;
import javax.swing.JPanel;

/**
 *
 * @author jflidr
 */
public class dcsNode extends JPanel implements MouseListener, MouseMotionListener {
    private Preferences prefs;
    
    private String id = "";
    private int cdx, cdy;
    private int nw, nh;
    private BufferedImage cd_img = null;
    private BufferedImage cd_imgh = null;
    private BufferedImage cd_img_sm = null;
    private BufferedImage cd_imgh_sm = null;
    private BufferedImage cd_img_inact = null;
    private BufferedImage cd_img_sm_inact = null;
    
    private String sysName = "";
    private String rtrID = "";
    
    private dcsNodeLabel nodeLabel = null;
    
    private int mouseX = 0;
    private int mouseY = 0;
    
    private boolean isHighlighted = false;
    private boolean isLabeled = false;
    private boolean nodeDragged = false;
    
    private Vector<String> teLinks = new Vector<String>(4);
    private Vector<dcsFiber> nodeFibers = new Vector<dcsFiber>();
    
    private Vector<dcsEROHop> nodeEROHops = new Vector<dcsEROHop>();
    private Vector<dcsLSP> nodeLSPs = new Vector<dcsLSP>();
    
    /** Creates a new instance of dcsNode */
    public dcsNode(String s, int nx, int ny, String r) {
        sysName = s;
        rtrID = r;
        cdx = nx; cdy = ny;
        int f = dcsGlobals.currMapPane.isZoomedOut()?2:1;
        
        prefs = dcsGlobals.prefsRoot.node("/edu/internet2/dcs/DcsMap/Nodes/"+s);
        
        loadSetGraphics();
        formatNode();
        
        setBackground(Color.BLUE);
        
        id = prefs.name();
        prefs.put("id", id);
        
        prefs.putInt("center_x", cdx*f);
        prefs.putInt("center_y", cdy*f);
        
        nodeLabel = new dcsNodeLabel();
        
        addMouseListener(this);
        addMouseMotionListener(this);
    }
    
    protected void paintComponent(Graphics g) {
        Graphics2D g2d = (Graphics2D) g;
        
        g2d.setRenderingHint(RenderingHints.KEY_ANTIALIASING,
                RenderingHints.VALUE_ANTIALIAS_ON);
        g2d.setRenderingHint(RenderingHints.KEY_RENDERING,
                RenderingHints.VALUE_RENDER_QUALITY);
        if(isHighlighted) {
            if(dcsGlobals.currMapPane.isZoomedOut()) {
                if(cd_imgh_sm != null) {
                    g2d.drawImage(cd_imgh_sm,0,0,this);
                }
            } else {
                if(cd_imgh != null) {
                    g2d.drawImage(cd_imgh,0,0,this);
                }
            }
        } else {
            if(dcsGlobals.currMapPane.isZoomedOut()) {
                if(sysName.equals("TULS")) {
                    if(cd_img_sm_inact != null) {
                        g2d.drawImage(cd_img_sm_inact,0,0,this);
                    }
                } else {
                    if(cd_img_sm != null) {
                        g2d.drawImage(cd_img_sm,0,0,this);
                    }
                }
            } else {
                if(sysName.equals("TULS")) {
                    if(cd_img_inact != null) {
                        g2d.drawImage(cd_img_inact,0,0,this);
                    }
                } else {
                    if(cd_img != null) {
                        g2d.drawImage(cd_img,0,0,this);
                    }
                }
            }
        }
    }
    
//getters
    public String getID() {
        return id;
    }
    
    public String getSysName() {
        return sysName;
    }
    
    public int getCenterX() {
        return cdx;
    }
    
    public int getCenterY() {
        return cdy;
    }
    
    public Vector<String> getTELinkLabels() {
        return teLinks;
    }
    
    public dcsNodeLabel getLabel() {
        return nodeLabel;
    }
    
    public Vector<dcsEROHop> getEROEndPts() {
        boolean notEP;
        Vector<dcsEROHop> ret = new Vector<dcsEROHop>();
        int i = 0;
        String id0;
        for (int j = 0; j < nodeEROHops.size(); j++) {
            ret.add(nodeEROHops.get(j));
        }
        
        while(i < ret.size()) {
            id0 = ret.get(i).getID();
            notEP = false;
            for (int j = i+1; j < ret.size(); j++) {
                if(id0.equals(ret.get(j).getID())) {
                    notEP = true;
                    break;
                }
            }
            if(notEP) {
                for (int j = (ret.size()-1); j >= i; j--) {
                    if(id0.equals(ret.get(j).getID())) {
                        ret.remove(j);
                    }
                }
            } else {
                i++;
            }
        }
        return ret;
    }
    
//setters
    public void setCenterX(int x) {
        cdx = x;
    }
    
    public void setCenterY(int y) {
        cdy = y;
    }
    
    public void updateCoordsPrefs() {
        int f = dcsGlobals.currMapPane.isZoomedOut()?2:1;
        prefs.putInt("center_x", cdx*f);
        prefs.putInt("center_y", cdy*f);
    }
    
    public void setHighlight(boolean b) {
        boolean emph = false;
        isHighlighted = b;
        dcsGlobals.dcsEROHops.highlightNodeEROs(this, b);
        if(!dcsGlobals.currMapPane.isZoomedOut())
            nodeLabel.setVisible(!b);
    }
    
    public void addTELink(String s) {
        teLinks.add(s);
    }
    
//queries
    public boolean isHighlighted() {
        return isHighlighted;
    }
    
//visual methods
    void moveBy(int dx, int dy) {
        if(((cdx-dx-nw/2)>=0) && ((cdx-dx+nw/2)<=dcsGlobals.currMapPane.getWidth())) {
            cdx -= dx;
        }
        if(((cdy-dy-nh/2)>=0) && ((cdy-dy+nh/2)<=dcsGlobals.currMapPane.getHeight())) {
            cdy -= dy;
        }
        setLocation(cdx-nw/2, cdy-nh/2);
        nodeLabel.setLocation();
    }
    
    public void toggleHighlight() {
        boolean emph = false;
        if(isHighlighted) {
            setHighlight(false);
        } else {
            int n = dcsGlobals.dcsNodes.getNumHihlights();
            if(n>=2) {
                dcsGlobals.dcsNodes.normHighlits();
            }
            setHighlight(true);
        }
    }
    
    public void loadSetGraphics() {
        URL img_url = dcsApplt.class.getResource("/edu/internet2/dcs/images/cd2.png");
        Image img = Toolkit.getDefaultToolkit().getImage(img_url);
        try {
            MediaTracker tracker = new MediaTracker(this);
            tracker.addImage(img, 0);
            tracker.waitForID(0);
        } catch ( Exception e ) {}
        
        img_url = dcsApplt.class.getResource("/edu/internet2/dcs/images/cd2h.png");
        Image imgh = Toolkit.getDefaultToolkit().getImage(img_url);
        try {
            MediaTracker tracker = new MediaTracker(this);
            tracker.addImage(imgh, 0);
            tracker.waitForID(0);
        } catch ( Exception e ) {}
        
        img_url = dcsApplt.class.getResource("/edu/internet2/dcs/images/cd2_sm.png");
        Image img_sm = Toolkit.getDefaultToolkit().getImage(img_url);
        try {
            MediaTracker tracker = new MediaTracker(this);
            tracker.addImage(img_sm, 0);
            tracker.waitForID(0);
        } catch ( Exception e ) {}
        
        img_url = dcsApplt.class.getResource("/edu/internet2/dcs/images/cd2h_sm.png");
        Image imgh_sm = Toolkit.getDefaultToolkit().getImage(img_url);
        try {
            MediaTracker tracker = new MediaTracker(this);
            tracker.addImage(imgh_sm, 0);
            tracker.waitForID(0);
        } catch ( Exception e ) {}
        
        img_url = dcsApplt.class.getResource("/edu/internet2/dcs/images/cd2_inact.png");
        Image img_inact = Toolkit.getDefaultToolkit().getImage(img_url);
        try {
            MediaTracker tracker = new MediaTracker(this);
            tracker.addImage(img_inact, 0);
            tracker.waitForID(0);
        } catch ( Exception e ) {}
        
        img_url = dcsApplt.class.getResource("/edu/internet2/dcs/images/cd2_sm_inact.png");
        Image img_sm_inact = Toolkit.getDefaultToolkit().getImage(img_url);
        try {
            MediaTracker tracker = new MediaTracker(this);
            tracker.addImage(img_sm_inact, 0);
            tracker.waitForID(0);
        } catch ( Exception e ) {}
        
        cd_img = new BufferedImage(img.getWidth(this), img.getHeight(this), BufferedImage.TYPE_INT_ARGB);
        if(cd_img != null) {
            Graphics2D g2d = cd_img.createGraphics();
            g2d.drawImage(img, 0, 0, this);
        }
        cd_imgh = new BufferedImage(imgh.getWidth(this), imgh.getHeight(this), BufferedImage.TYPE_INT_ARGB);
        if(cd_imgh != null) {
            Graphics2D g2d = cd_imgh.createGraphics();
            g2d.drawImage(imgh, 0, 0, this);
        }
        cd_img_sm = new BufferedImage(img_sm.getWidth(this), img_sm.getHeight(this), BufferedImage.TYPE_INT_ARGB);
        if(cd_img_sm != null) {
            Graphics2D g2d = cd_img_sm.createGraphics();
            g2d.drawImage(img_sm, 0, 0, this);
        }
        cd_imgh_sm = new BufferedImage(imgh_sm.getWidth(this), imgh_sm.getHeight(this), BufferedImage.TYPE_INT_ARGB);
        if(cd_imgh_sm != null) {
            Graphics2D g2d = cd_imgh_sm.createGraphics();
            g2d.drawImage(imgh_sm, 0, 0, this);
        }
        cd_img_inact = new BufferedImage(img.getWidth(this), img.getHeight(this), BufferedImage.TYPE_INT_ARGB);
        if(cd_img_inact != null) {
            Graphics2D g2d = cd_img_inact.createGraphics();
            g2d.drawImage(img_inact, 0, 0, this);
        }
        cd_img_sm_inact = new BufferedImage(img_sm.getWidth(this), img_sm.getHeight(this), BufferedImage.TYPE_INT_ARGB);
        if(cd_img_sm_inact != null) {
            Graphics2D g2d = cd_img_sm_inact.createGraphics();
            g2d.drawImage(img_sm_inact, 0, 0, this);
        }
    }
    
    public void formatNode() {
        if(dcsGlobals.currMapPane.isZoomedOut()) {
            nw = cd_img_sm.getWidth(this);
            nh = cd_img_sm.getHeight(this);
        } else {
            nw = cd_img.getWidth(this);
            nh = cd_img.getHeight(this);
        }
        setSize(nw, nh);
        setLocation(((cdx-nw/2)<0)?0:cdx-nw/2, ((cdy-nh/2)<0)?0:cdy-nh/2);
        if(nodeLabel != null) {
            nodeLabel.setLocation();
            if(dcsGlobals.currMapPane.isZoomedOut()) {
                nodeLabel.setVisible(true);
            } else {
                if(isHighlighted) {
                    nodeLabel.setVisible(false);
                }
            }
        }
    }
    
    public void scaleDown() {
        cdx /= 2;
        cdy /= 2;
    }
    
    public void scaleUp() {
        cdx *= 2;
        cdy *= 2;
    }
    
// infrastructure methods
    public void addFiber(dcsFiber f) {
        nodeFibers.add(f);
    }
    
    public void addEROHop(dcsEROHop e) {
        nodeEROHops.add(e);
    }
    
    public void removeEROHop(dcsEROHop e) {
        nodeEROHops.remove(e);
    }
    
    public void addLSP(dcsLSP l) {
        nodeLSPs.add(l);
    }
    
    public void removeLSP(dcsLSP l) {
        nodeLSPs.remove(l);
    }
    
    public void mouseClicked(MouseEvent e) {
        toggleHighlight();
        nodeDragged = false;
        dcsGlobals.currMapPane.repaint();
    }
    
    public void mousePressed(MouseEvent e) {
        mouseX = e.getX();
        mouseY = e.getY();
        nodeDragged = false;
    }
    
    public void mouseReleased(MouseEvent e) {
        if(nodeDragged) {
            updateCoordsPrefs();
            nodeDragged = false;
        }
    }
    
    public void mouseEntered(MouseEvent e) {
    }
    
    public void mouseExited(MouseEvent e) {
    }
    
    public void mouseDragged(MouseEvent e) {
        Point p;
        if(isPopUp(e)) {
            return;
        }
        
        moveBy(mouseX - e.getX(), mouseY - e.getY());
        dcsGlobals.currMapPane.repaint();
        nodeDragged = true;
    }
    
    public void mouseMoved(MouseEvent e) {
    }
    
    private boolean isPopUp(MouseEvent e) {
        return (e.isPopupTrigger() ||
                ((e.getModifiersEx() & e.CTRL_DOWN_MASK) == e.CTRL_DOWN_MASK));
    }
    
// private classes
    
    private class dcsNodeLabel extends JPanel {
        private boolean isEmph = false;
        private final Color NODE_TIP_FONT_COLOR = new Color(0, 0, 0, 255);
        private final Color NODE_TIP_EM_FONT_COLOR = new Color(222, 0, 0, 255);
        
        public dcsNodeLabel() {
            setOpaque(false);
            setVisible(true);
            setLocation();
            setSize(70, 20);
        }
        
        public void paintComponent(Graphics g) {
            if(isVisible()) {
                Graphics2D g2d = (Graphics2D)g;
                Font f = new Font("Courier", Font.PLAIN, 10);
                Rectangle2D sb = f.getStringBounds(rtrID, g2d.getFontRenderContext());
                
                g2d.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
                
                if(isEmph)
                    g2d.setPaint(NODE_TIP_EM_FONT_COLOR);
                else
                    g2d.setPaint(NODE_TIP_FONT_COLOR);
                g2d.setFont(f);
                
                setLocation(cdx - nw/2 + 5, cdy + nh/2);
                if(dcsGlobals.currMapPane.isZoomedOut()) {
                    setSize(4 + (int)sb.getWidth(), (int)sb.getHeight());
                    g2d.drawString(sysName, 2, (int)sb.getHeight());
                } else {
                    setSize(4 + (int)sb.getWidth(), 2*((int)sb.getHeight()) + 2);
                    g2d.drawString(sysName, 2, (int)sb.getHeight());
                    g2d.drawString(rtrID, 2, 2*((int)sb.getHeight()) + 2);
                }
            }
        }
        
        public void setLocation() {
            setLocation(cdx - nw/2 + 5, cdy + nh/2);
        }
    }
}
