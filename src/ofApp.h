#pragma once

//addons
#include "ofMain.h"
#include "ofxOpenNI.h"
#include "ofxUI.h"
#include "ofxVideoRecorder.h"

//classes
#include "KinectMotor.h"
#include <sndfile.hh>

#define DRAW_RGB 1
#define DRAW_DEPTH 2
#define DRAW_INFRA 3

class ofApp : public ofBaseApp{
    
public:
    void setup();
    void update();
    void draw();
    
    void keyPressed  (int key);
    void keyReleased(int key);
    void mouseMoved(int x, int y );
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void windowResized(int w, int h);
    void dragEvent(ofDragInfo dragInfo);
    void gotMessage(ofMessage msg);
    
    void audioReceived 	(float * input, int bufferSize, int nChannels);
    void exit();
    
    //GUI
    void setupGui();
    void guiEvent(ofxUIEventArgs &e);
    ofxUICanvas *gui;
    string fileList;
    
    bool recording;
    
    //Kinect
    ofxOpenNI kinect;
    int drawMode, currentFrame;
    bool drawSkeletons;
    KinectMotor motor;
    bool hadUsers;
    
    //Mocap
    bool recordTsv;
    ofFile* tsvFile;
    map<string, ofFile*> repovizzFiles;
    
    //Audio
    int sampleRate, channels;
    bool aligned;
    SndfileHandle *audioFile;
    SF_INFO info;
    
    //Played sound
    ofSoundPlayer player;
    bool audio, recordAudio;
    string statusAudio;
    
    //Video recording
    ofxVideoRecorder rgbVideoRecorder, depthVideoRecorder;
    bool recordVideo, recordOni;
    unsigned long recordingStartTime;
    
private:
    void finishRecording();
    void updateDir();
    void writeTsvHeader();
    void writeBonesFile();
    void writeRepovizzXml();
    
};
