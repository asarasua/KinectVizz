#include "ofApp.h"
#include <sndfile.hh>

//--------------------------------------------------------------
void ofApp::setup(){
    ofSetLogLevel(OF_LOG_VERBOSE);
    ofBackground(75,75,75);
    
    //Motor
    motor.Open();
    motor.Move(0);
    
    //Kinect
    kinect.setup();
    kinect.setLogLevel(OF_LOG_VERBOSE);
    kinect.addDepthGenerator();
    kinect.addImageGenerator();
    kinect.setRegister(true);
    kinect.addUserGenerator();
    kinect.setMaxNumUsers(5);
    kinect.start();
    currentFrame = 0;
    
    recording = false;
    
    ofSoundStreamSetup(0,2,this, 44100, 256, 4);
    statusAudio = "No sound loaded!";
    
    setupGui();
    
    //Audio rec
    channels = 1;
    sampleRate = 44100;
}

//--------------------------------------------------------------
void ofApp::update(){
    kinect.update();
    
    //Workaround to reset skeleton tracker when all users are lost
    if (!kinect.getNumTrackedUsers() && hadUsers){
        hadUsers = false;
        kinect.setPaused(true);
        kinect.removeUserGenerator();
        kinect.addUserGenerator();
        kinect.setPaused(false);
    } else if (kinect.getNumTrackedUsers()){
        hadUsers = true;
    }
    
    //Finished playing
    if (audio && !player.getIsPlaying() && ((ofxUILabelToggle*) gui->getWidget("PLAY"))->getValue()){
        ((ofxUILabelToggle*) gui->getWidget("PLAY"))->setValue(false);
        ((ofxUILabelToggle*) gui->getWidget("REC"))->setValue(false);
        if (recording){
            finishRecording();
        }
    }
    
    //Record data
    if (recording){
        if (kinect.getUserGenerator().GetFrameID() > currentFrame){
            currentFrame = kinect.getUserGenerator().GetFrameID();
            
            //Mocap
            for (int i = 0; i  < kinect.getNumTrackedUsers(); ++i){
                ofxOpenNIUser user = kinect.getTrackedUser(i);
                if (recordTsv) {
                    //TSV
                    if (audio){
                        *tsvFile << kinect.getUserGenerator().GetFrameID() << "\t" << user.getXnID() << "\t" << player.getPositionMS()<< "\t" << ofGetTimestampString() << "\t";
                    } else {
                        *tsvFile << kinect.getUserGenerator().GetFrameID() << "\t" << user.getXnID() << "\t" << ofGetElapsedTimeMillis() - recordingStartTime << "\t" << ofGetTimestampString() << "\t";
                    }
                }
                
                for (int j = 0; j < user.getNumJoints(); ++j) {
                    ofxOpenNIJoint joint = user.getJoint((Joint)j);
                    
                    //TSV
                    if (recordTsv) {
                        *tsvFile << joint.getWorldPosition().x << "\t" << joint.getWorldPosition().y << "\t" << joint.getWorldPosition().z << "\t";
                    }
                    
                    //Repovizz files
                    if (repovizzFiles.find(joint.getName()+"x") == repovizzFiles.end()) {
                        string folderName = ((ofxUITextInput*) gui->getWidget("FOLDERNAME"))->getTextString() + "/";
                        repovizzFiles[joint.getName() + "x"] = new ofFile(folderName + joint.getName() + "[0].csv", ofFile::WriteOnly);
                        repovizzFiles[joint.getName() + "y"] = new ofFile(folderName + joint.getName() + "[1].csv", ofFile::WriteOnly);
                        repovizzFiles[joint.getName() + "z"] = new ofFile(folderName + joint.getName() + "[2].csv", ofFile::WriteOnly);
                        *repovizzFiles[joint.getName() + "x"] << "repoVizz,category=categorytest,name=nametest,offsettime=0,framerate=30,minval=-5000,maxval=5000," << endl;
                        *repovizzFiles[joint.getName() + "y"] << "repoVizz,category=categorytest,name=nametest,offsettime=0,framerate=30,minval=-5000,maxval=5000," << endl;
                        *repovizzFiles[joint.getName() + "z"] << "repoVizz,category=categorytest,name=nametest,offsettime=0,framerate=30,minval=-5000,maxval=5000," << endl;
                    }
                    
                    *repovizzFiles[joint.getName() + "x"] << joint.getWorldPosition().x << ",";
                    *repovizzFiles[joint.getName() + "y"] << joint.getWorldPosition().y << ",";
                    *repovizzFiles[joint.getName() + "z"] << joint.getWorldPosition().z << ",";
                }
                if (recordTsv) {
                    *tsvFile << endl;
                }
            }
            
            //Video
            if (recordVideo) {
                rgbVideoRecorder.addFrame(kinect.getImagePixels());
                depthVideoRecorder.addFrame(kinect.getDepthPixels());
            }
        }
    }
}

//--------------------------------------------------------------
void ofApp::draw(){
	ofSetColor(255, 255, 255);
    
    //fileList
    ofDrawBitmapString(fileList, gui->getWidget("EXISTING RECORDINGS")->getRect()->getX(), gui->getWidget("EXISTING RECORDINGS")->getRect()->getY()+40);
    
    //status
    ofDrawBitmapString(statusAudio, gui->getWidget("STATUS")->getRect()->getX(), gui->getWidget("STATUS")->getRect()->getY()+40);
    if (kinect.getNumTrackedUsers() == 0) {
        ofDrawBitmapString("no users tracked!", gui->getWidget("STATUS")->getRect()->getX(), gui->getWidget("STATUS")->getRect()->getY()+70);
    } else {
        ofSetColor(ofColor::green);
        ofDrawBitmapString("tracking " + ofToString(kinect.getNumTrackedUsers()) + " user(s)", gui->getWidget("STATUS")->getRect()->getX(), gui->getWidget("STATUS")->getRect()->getY()+70);
        ofSetColor(255, 255, 255);
    }
    if (recording) {
        ofSetColor(ofColor::red);
        ofDrawBitmapString("recording to " + ((ofxUITextInput*) gui->getWidget("FOLDERNAME"))->getTextString(), gui->getWidget("STATUS")->getRect()->getX(), gui->getWidget("STATUS")->getRect()->getY()+100);
        ofSetColor(255, 255, 255);
    } else {
        ofDrawBitmapString("not recording", gui->getWidget("STATUS")->getRect()->getX(), gui->getWidget("STATUS")->getRect()->getY()+100);
    }
    
    //Kinect input
    switch (drawMode) {
        case DRAW_RGB:
            kinect.drawImage(gui->getWidget("INPUT")->getRect()->getX(), gui->getWidget("INPUT")->getRect()->getY(), 600, 450);
            break;
        case DRAW_DEPTH:
            kinect.drawDepth(gui->getWidget("INPUT")->getRect()->getX(), gui->getWidget("INPUT")->getRect()->getY(), 600, 450);
            break;
        default:
            break;
    }
    
    if (drawSkeletons) {
        kinect.drawSkeletons(gui->getWidget("INPUT")->getRect()->getX(), gui->getWidget("INPUT")->getRect()->getY(), 600, 450);
    }
    
    if (kinect.getNumTrackedUsers()){
        ofxOpenNIUser user = kinect.getTrackedUser(0);
        for (int i = 0; i < user.getNumJoints(); i++) {
            ofDrawBitmapString(user.getJoint((Joint)i).getName(), user.getJoint((Joint)i).getProjectivePosition());
        }
    }
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){
    gui->setPosition(w/2 - gui->getRect()->getWidth() / 2, gui->getRect()->getY());
}

//--------------------------------------------------------------
void ofApp::exit(){
	kinect.stop();
    player.stop();
    if (recording) {
        finishRecording();
    }
}

//--------------------------------------------------------------

void ofApp::setupGui(){
    //GUI
    gui = new ofxUICanvas(50, 50, ofGetWidth()-100, ofGetHeight()-100);
    gui->setDrawBack(false);
    
    gui->addWidgetDown(new ofxUILabel("OPENNI RECORDER", OFX_UI_FONT_LARGE));
    gui->addSpacer();
    
    //Container of input image(s)
    gui->addSpacer(600, 450, "INPUT");
    ((ofxUISpacer*) gui->getWidget("INPUT"))->setDrawFill(false);
    ((ofxUISpacer*) gui->getWidget("INPUT"))->setDrawOutline(true);
    
    //Radio to select img mode
    vector<string> drawmodes;
	drawmodes.push_back("RGB");
	drawmodes.push_back("DEPTH");
    gui->addRadio("DRAW_MODE_RADIO", drawmodes, OFX_UI_ORIENTATION_HORIZONTAL);
    ((ofxUIRadio*) gui->getWidget("DRAW_MODE_RADIO"))->activateToggle(drawmodes[0]);
    drawMode = DRAW_RGB;
    
    //Show skeletons toggle
    gui->addWidgetDown(new ofxUILabelToggle("SKELETONS", true, 90, 10, 0, 0, OFX_UI_FONT_SMALL));
    drawSkeletons = true;
    
    gui->addWidgetEastOf(new ofxUILabelToggle("AUDIO", false, 90, 10, 0, 0, OFX_UI_FONT_SMALL), "SKELETONS");
    audio = false;
    
    gui->addWidgetEastOf(new ofxUILabelToggle("RECAUDIO", true, 90, 10, 0, 0, OFX_UI_FONT_SMALL), "AUDIO");
    recordAudio = true;
    
    gui->addWidgetEastOf(new ofxUILabelToggle("RECVIDEO", true, 90, 10, 0, 0, OFX_UI_FONT_SMALL), "RECAUDIO");
    recordVideo = true;
    
    gui->addWidgetEastOf(new ofxUILabelToggle("RECONI", false, 90, 10, 0, 0, OFX_UI_FONT_SMALL), "RECVIDEO");
    recordOni = false;
    
    gui->addWidgetEastOf(new ofxUILabelToggle("RECTSV", false, 90, 10, 0, 0, OFX_UI_FONT_SMALL), "RECONI");
    recordTsv = false;
    
    
    //Logos
    ofImage *esmuc = new ofImage();
    esmuc->loadImage("GUI/esmuc.jpg");
    gui->addImage("", esmuc, .14*esmuc->getWidth(), .14*esmuc->getHeight());
    ofImage *mtg = new ofImage();
    mtg->loadImage("GUI/mtg.png");
    gui->addWidgetRight(new ofxUIImage(.09*esmuc->getWidth(), .14*esmuc->getHeight(), mtg, ""));
    
    //Tilt motor control
    gui->addWidgetEastOf(new ofxUISlider("TILT", -31.0, 31.0, 0.0, 10, 450), "INPUT");
    
    //Audio selection
    ofDirectory dir("");
    dir.allowExt("mp3");
    dir.allowExt("wav");
    dir.listDir();
    
    vector<string> audios;
    
    for (int i = 0; i < dir.numFiles(); i++){
        audios.push_back(dir.getPath(i));
    }
    
    gui->addWidgetEastOf(new ofxUIRadio("AUDIOS", audios, OFX_UI_ORIENTATION_VERTICAL, 10, 10), "TILT");
    
    //Playback and Record control
    gui->addWidgetSouthOf(new ofxUILabelToggle("PLAY", false, 235, 10),"AUDIOS");
    gui->addWidgetSouthOf(new ofxUILabelToggle("PAUSE", false, 235, 10), "PLAY");
    gui->addWidgetSouthOf(new ofxUILabelToggle("STOP", false, 235, 10), "PAUSE");
    gui->addWidgetSouthOf(new ofxUILabelToggle("REC", false, 235, 10), "STOP");
    gui->getWidget("REC")->setColorBack(ofxUIColor::red);
    
    //Filename
    gui->addWidgetSouthOf(new ofxUISpacer(235, 40, "SP1"), "REC");
    gui->getWidget("SP1")->setDrawFill(false);
    gui->getWidget("SP1")->setDrawOutline(false);
    
    gui->addWidgetSouthOf(new ofxUITextInput("FOLDERNAME", ofGetTimestampString(), 235), "SP1");
    gui->getWidget("FOLDERNAME")->setColorBack(ofxUIColor::gray);
    
    //Dir
    gui->addWidgetSouthOf(new ofxUISpacer(235, 40, "SP2"), "FOLDERNAME");
    gui->getWidget("SP2")->setDrawFill(false);
    gui->getWidget("SP2")->setDrawOutline(false);
    
    gui->addWidgetSouthOf(new ofxUILabel("EXISTING RECORDINGS", OFX_UI_FONT_MEDIUM), "SP2");
    
    updateDir();
    
    //Status
    gui->addWidgetSouthOf(new ofxUISpacer(235, 220, "SP3"), "EXISTING RECORDINGS");
    gui->getWidget("SP3")->setDrawFill(false);
    gui->getWidget("SP3")->setDrawOutline(false);
    
    gui->addWidgetSouthOf(new ofxUILabel("STATUS", OFX_UI_FONT_MEDIUM), "SP3");
    
    ofAddListener(gui->newGUIEvent,this,&ofApp::guiEvent);
}

//--------------------------------------------------------------
void ofApp::guiEvent(ofxUIEventArgs &e){
	string name = e.widget->getName();
	//cout << "got event from: " << name << endl;
    
    //KINECT TILT
    if (name == "TILT") {
        ofxUISlider *tiltSlider = (ofxUISlider*) e.widget;
        motor.Move((int)tiltSlider->getScaledValue());
    }
    //KINECT VISUALIZATION CONTROL
    else if (name == "RGB"){
        drawMode = DRAW_RGB;
    }
    else if (name == "DEPTH"){
        drawMode = DRAW_DEPTH;
    }
    else if (name == "SKELETONS"){
        drawSkeletons = ((ofxUILabelToggle*) e.widget)->getValue();
    }
    else if (name == "AUDIO"){
        audio = ((ofxUILabelToggle*) e.widget)->getValue();
    }
    else if (name == "RECAUDIO"){
        recordAudio = ((ofxUILabelToggle*) e.widget)->getValue();
    }
    else if (name == "RECVIDEO"){
        recordVideo = ((ofxUILabelToggle*) e.widget)->getValue();
    }
    else if (name == "RECONI"){
        recordOni = ((ofxUILabelToggle*) e.widget)->getValue();
    }
    else if (name == "RECTSV"){
        recordTsv = ((ofxUILabelToggle*) e.widget)->getValue();
    }
    //AUDIO PLAYBACK
    else if (name == "PLAY"){
        if (((ofxUILabelToggle*) e.widget)->getValue()) {
            ((ofxUILabelToggle*) gui->getWidget("PAUSE"))->setValue(false);
            ((ofxUILabelToggle*) gui->getWidget("STOP"))->setValue(false);
            player.play();
        }
    }
    else if (name == "PAUSE"){
        player.setPaused(((ofxUILabelToggle*) e.widget)->getValue());
    }
    else if (name == "STOP"){
        if (((ofxUILabelToggle*) e.widget)->getValue()){
            player.stop();
            ((ofxUILabelToggle*) gui->getWidget("PLAY"))->setValue(false);
            ((ofxUILabelToggle*) gui->getWidget("PAUSE"))->setValue(false);
            ((ofxUILabelToggle*) gui->getWidget("REC"))->setValue(false);
            
            if (recording) {
                finishRecording();
            }
        }
    }
    else if (name == "REC"){
        recording = ((ofxUILabelToggle*) e.widget)->getValue();
        if (recording){
            recordingStartTime = ofGetElapsedTimeMillis();
            
            //Play audio
            ((ofxUILabelToggle*) gui->getWidget("STOP"))->setValue(false);
            if (!player.getIsPlaying()) {
                ((ofxUILabelToggle*) gui->getWidget("PLAY"))->setValue(true);
                player.play();
            }
            
            string folderName = ((ofxUITextInput*) gui->getWidget("FOLDERNAME"))->getTextString() + "/";
            
            //Bones and xmlfile
            writeBonesFile();
            writeRepovizzXml();
            
            //TSV FILE
            if (recordTsv) {
                tsvFile = new ofFile(folderName + "mocap.tsv", ofFile::WriteOnly);
                writeTsvHeader();
            }
            
            //AUDIO FILE
            if (recordAudio) {
                string audioFileName = folderName + "audio.wav";
                aligned = false;
                ofDirectory dir("./");
                //audioFile = sf_open ((dir.getAbsolutePath() + "/" + audioFileName).c_str(), SFM_WRITE, &info);
                audioFile = new SndfileHandle((dir.getAbsolutePath() + "/" + audioFileName).c_str(), SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_PCM_16, channels, sampleRate);
                
                if (!audioFile)
                {
                    cerr << "Error opening ["<< audioFileName<< "] : " << audioFile->strError() << endl;
                }
            }
            
            //VIDEO FILES
            if (recordVideo) {
                rgbVideoRecorder.setVideoCodec("mpeg4");
                rgbVideoRecorder.setVideoBitrate("800k");
                rgbVideoRecorder.setup(folderName + "rgb.mov", kinect.getWidth(), kinect.getHeight(), 30, sampleRate, 0);
                depthVideoRecorder.setVideoCodec("mpeg4");
                depthVideoRecorder.setVideoBitrate("800k");
                depthVideoRecorder.setPixelFormat("rgba");
                depthVideoRecorder.setup(folderName + "rgbd.mov", kinect.getWidth(), kinect.getHeight(), 30, sampleRate, 0);
            }
            
            //ONI FILE
            if (recordOni) {
                kinect.startRecording(folderName + "oniFile.oni");
            }
        } else {
            finishRecording();
        }
    }
    else if (name.find("mp3") != string::npos || name.find(".wav") != string::npos){
        ((ofxUILabelToggle*) gui->getWidget("PLAY"))->setValue(false);
        ((ofxUILabelToggle*) gui->getWidget("PAUSE"))->setValue(false);
        ((ofxUILabelToggle*) gui->getWidget("STOP"))->setValue(false);
        ((ofxUILabelToggle*) gui->getWidget("REC"))->setValue(false);
        player.loadSound(name);
        statusAudio = "Loaded sound: " + name;
    }
}

//--------------------------------------------------------------
void ofApp::finishRecording(){
    //REPOVIZZ Files
    for (map<string, ofFile*>::iterator it = repovizzFiles.begin(); it != repovizzFiles.end(); it++) {
        delete it->second;
    }
    repovizzFiles.clear();
    
    //TSV
    if (recordTsv) {
        delete tsvFile;
    }
    
    //COPY AUDIO PLAYED DURING RECORDING
    if (audio) {
        ofFile audio;
        string folderName = ((ofxUITextInput*) gui->getWidget("FOLDERNAME"))->getTextString() + "/";
        string audioName = ((ofxUIRadio*) gui->getWidget("AUDIOS"))->getActiveName();
        audio.open(audioName, ofFile::ReadWrite, true);
        audio.copyTo(folderName + audioName);
    }
    
    //AUDIO FILE
    if (recordAudio) {
        delete audioFile;
    }
    
    //VIDEO FILE
    if (recordVideo) {
        rgbVideoRecorder.close();
        depthVideoRecorder.close();
    }
    
    //ONI FILE
    if (recordOni) {
        kinect.stopRecording();
    }
    
    recording = false;
    updateDir();
    ((ofxUITextInput*) gui->getWidget("FOLDERNAME"))->setTextString(ofGetTimestampString());
}

//--------------------------------------------------------------
void ofApp::updateDir(){
    fileList = "";
    ofDirectory dir("");
    dir.allowExt("");
    dir.listDir();
    
    for (int i = 0; i < dir.numFiles(); i++){
        if (dir.getPath(i) != "GUI" && dir.getPath(i) != "openni" && dir.getPath(i).find("ofx") == string::npos) {
            fileList += dir.getPath(i) + "\n";
        }
    }
}

//--------------------------------------------------------------
void ofApp::writeTsvHeader(){
    if (kinect.getNumTrackedUsers()){
        *tsvFile << "MARKER_NAMES\t";
        
        for (int i = 0; i < kinect.getTrackedUser(0).getNumJoints(); ++i) {
            *tsvFile << kinect.getTrackedUser(0).getJoint((Joint)i).getName() << "\t";
        }
        
        *tsvFile << endl << "Frame\tid\tTime\tSMPTE\t";
        for (int i = 0; i < kinect.getTrackedUser(0).getNumJoints(); ++i) {
            *tsvFile << kinect.getTrackedUser(0).getJoint((Joint)i).getName() << " X\t";
            *tsvFile << kinect.getTrackedUser(0).getJoint((Joint)i).getName() << " Y\t";
            *tsvFile << kinect.getTrackedUser(0).getJoint((Joint)i).getName() << " Z\t";
        }
        *tsvFile << endl;
    }
}

//--------------------------------------------------------------
void ofApp::audioReceived 	(float * input, int bufferSize, int nChannels){
    if (recordAudio && recording && audioFile) {
        if (!aligned) {
            aligned = true;
            unsigned long delay = ofGetElapsedTimeMillis() - recordingStartTime;
            const int size = sampleRate*(delay/1000.0);
            float sample[size];
            float current=0.;
            for (int i=0; i<size; i++) sample[i]=0.0;
            audioFile->write(&sample[0], size);
        }
        
        audioFile->write(input, bufferSize);
    }
}

//--------------------------------------------------------------
void ofApp::writeBonesFile(){
    string folderName = ((ofxUITextInput*) gui->getWidget("FOLDERNAME"))->getTextString() + "/";
    ofFile bonesFile;
    bonesFile.open(folderName + "KinectConnections.bones", ofFile::WriteOnly);
    bonesFile << "A, kinect\nM, ClosestHand, 0, 255, 0\nM, Head, 255, 255, 0, 0.02\nM, LeftElbow, 0, 255, 0\nM, LeftFoot, 255, 255, 0\nM, LeftHand, 255, 150, 100\nM, LeftHip, 0, 255, 0\nM, LeftKnee, 0, 255, 0\nM, LeftShoulder, 0, 255, 0\nM, RightElbow, 0, 255, 0\nM, RightFoot, 255, 255, 0\nM, RightHand, 255, 150, 100\nM, RightHip, 0, 255, 0\nM, RightKnee, 0, 255, 0\nM, RightShoulder, 0, 255, 0\nM, Torso, 0, 255, 0\nB, LeftShoulder, RightShoulder\nB, Head, Torso\nB, RightShoulder, RightElbow\nB, LeftShoulder, LeftElbow\nB, RightElbow, RightHand\nB, LeftElbow, LeftHand\nB, RightShoulder, RightHip\nB, LeftShoulder, LeftHip\nB, RightHip, Torso\nB, LeftHip, Torso\nB, RightHip, LeftHip\nB, RightHip, RightKnee\nB, LeftHip, LeftKnee\nB, RightKnee, RightFoot\nB, LeftKnee, LeftFoot";
    bonesFile.close();
}

//--------------------------------------------------------------
void ofApp::writeRepovizzXml(){
    string folderName = ((ofxUITextInput*) gui->getWidget("FOLDERNAME"))->getTextString() + "/";
    ofFile xml;
    xml.open(folderName + "datapack.xml", ofFile::WriteOnly);
    xml << "<ROOT ID=\"ROOT0\"><Generic Category=\"MoCapGroup\" ID=\"ROOT0_MoCa0\" Name=\"Kinect Subject Markers\" _Extra=\"\" Expanded=\"1\"><File ID=\"ROOT0_MoCa0_MoCa0\" Expanded=\"1\" Category=\"MoCapLinks\" Name=\"Connections for Kinect Skeleton\" FileType=\"bones\" DefaultPath=\"0\" Filename=\"KinectConnections.bones\" _Extra=\"canvas=-1,color=0,selected=1\" /><Generic Name=\"Head\" _Extra=\"\" ID=\"ROOT0_MoCa0_MoCa1\" Category=\"MoCapMarker\" Expanded=\"0\"><Signal Category=\"X\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_HEAD[0].csv\" ID=\"ROOT0_MoCa0_MoCa1_X0\" MaxVal=\"\" MinVal=\"\" Name=\"X\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" BytesPerSample=\"\" FrameSize=\"\" NumChannels=\"\" NumSamples=\"\" SampleRate=\"30\" /><Signal Category=\"Y\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_HEAD[1].csv\" ID=\"ROOT0_MoCa0_MoCa1_Y0\" MaxVal=\"\" MinVal=\"\" Name=\"Y\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" BytesPerSample=\"\" FrameSize=\"\" NumChannels=\"\" NumSamples=\"\" SampleRate=\"30\" /><Signal Category=\"Z\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_HEAD[2].csv\" ID=\"ROOT0_MoCa0_MoCa1_Z0\" MaxVal=\"\" MinVal=\"\" Name=\"Z\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" BytesPerSample=\"\" FrameSize=\"\" NumChannels=\"\" NumSamples=\"\" SampleRate=\"30\" /></Generic><Generic ID=\"ROOT0_MoCa0_MoCa2\" Category=\"MoCapMarker\" Name=\"Neck\" _Extra=\"\" Expanded=\"0\"><Signal BytesPerSample=\"\" Category=\"X\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_NECK[0].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa2_X0\" MaxVal=\"\" MinVal=\"\" Name=\"X\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Y\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_NECK[1].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa2_Y0\" MaxVal=\"\" MinVal=\"\" Name=\"Y\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Z\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_NECK[2].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa2_Z0\" MaxVal=\"\" MinVal=\"\" Name=\"Z\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /></Generic><Generic Category=\"MoCapMarker\" ID=\"ROOT0_MoCa0_MoCa3\" Name=\"Torso\" _Extra=\"\" Expanded=\"0\"><Signal Category=\"X\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_TORSO[0].csv\" ID=\"ROOT0_MoCa0_MoCa3_X0\" MaxVal=\"\" MinVal=\"\" Name=\"X\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" BytesPerSample=\"\" FrameSize=\"\" NumChannels=\"\" NumSamples=\"\" SampleRate=\"30\" /><Signal Category=\"Y\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_TORSO[1].csv\" ID=\"ROOT0_MoCa0_MoCa3_Y0\" MaxVal=\"\" MinVal=\"\" Name=\"Y\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" BytesPerSample=\"\" FrameSize=\"\" NumChannels=\"\" NumSamples=\"\" SampleRate=\"30\" /><Signal Category=\"Z\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_TORSO[2].csv\" ID=\"ROOT0_MoCa0_MoCa3_Z0\" MaxVal=\"\" MinVal=\"\" Name=\"Z\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" BytesPerSample=\"\" FrameSize=\"\" NumChannels=\"\" NumSamples=\"\" SampleRate=\"30\" /></Generic><Generic Category=\"MoCapMarker\" ID=\"ROOT0_MoCa0_MoCa4\" Name=\"LeftShoulder\" _Extra=\"\" Expanded=\"0\"><Signal BytesPerSample=\"\" Category=\"X\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_LEFT_SHOULDER[0].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa4_X0\" MaxVal=\"\" MinVal=\"\" Name=\"X\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Y\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_LEFT_SHOULDER[1].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa4_Y0\" MaxVal=\"\" MinVal=\"\" Name=\"Y\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Z\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_LEFT_SHOULDER[2].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa4_Z0\" MaxVal=\"\" MinVal=\"\" Name=\"Z\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /></Generic><Generic Category=\"MoCapMarker\" ID=\"ROOT0_MoCa0_MoCa5\" Name=\"LeftElbow\" _Extra=\"\" Expanded=\"0\"><Signal BytesPerSample=\"\" Category=\"X\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_LEFT_ELBOW[0].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa5_X0\" MaxVal=\"\" MinVal=\"\" Name=\"X\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Y\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_LEFT_ELBOW[1].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa5_Y0\" MaxVal=\"\" MinVal=\"\" Name=\"Y\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Z\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_LEFT_ELBOW[2].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa5_Z0\" MaxVal=\"\" MinVal=\"\" Name=\"Z\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /></Generic><Generic Category=\"MoCapMarker\" ID=\"ROOT0_MoCa0_MoCa6\" Name=\"LeftHand\" _Extra=\"\" Expanded=\"0\"><Signal BytesPerSample=\"\" Category=\"X\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_LEFT_HAND[0].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa6_X0\" MaxVal=\"\" MinVal=\"\" Name=\"X\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Y\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_LEFT_HAND[1].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa6_Y0\" MaxVal=\"\" MinVal=\"\" Name=\"Y\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Z\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_LEFT_HAND[2].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa6_Z0\" MaxVal=\"\" MinVal=\"\" Name=\"Z\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /></Generic><Generic Category=\"MoCapMarker\" ID=\"ROOT0_MoCa0_MoCa7\" Name=\"RightShoulder\" _Extra=\"\" Expanded=\"0\"><Signal BytesPerSample=\"\" Category=\"X\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_RIGHT_SHOULDER[0].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa7_X0\" MaxVal=\"\" MinVal=\"\" Name=\"X\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Y\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_RIGHT_SHOULDER[1].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa7_Y0\" MaxVal=\"\" MinVal=\"\" Name=\"Y\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Z\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_RIGHT_SHOULDER[2].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa7_Z0\" MaxVal=\"\" MinVal=\"\" Name=\"Z\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /></Generic><Generic Category=\"MoCapMarker\" Expanded=\"0\" ID=\"ROOT0_MoCa0_MoCa8\" Name=\"RightElbow\" _Extra=\"\"><Signal BytesPerSample=\"\" Category=\"X\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_RIGHT_ELBOW[0].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa8_X0\" MaxVal=\"\" MinVal=\"\" Name=\"X\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SampleRate=\"30\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" /><Signal BytesPerSample=\"\" Category=\"Y\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_RIGHT_ELBOW[1].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa8_Y0\" MaxVal=\"\" MinVal=\"\" Name=\"Y\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SampleRate=\"30\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" /><Signal BytesPerSample=\"\" Category=\"Z\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_RIGHT_ELBOW[2].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa8_Z0\" MaxVal=\"\" MinVal=\"\" Name=\"Z\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SampleRate=\"30\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" /></Generic><Generic Category=\"MoCapMarker\" ID=\"ROOT0_MoCa0_MoCa9\" Name=\"RightHand\" _Extra=\"\" Expanded=\"0\"><Signal BytesPerSample=\"\" Category=\"X\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_RIGHT_HAND[0].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa9_X0\" MaxVal=\"\" MinVal=\"\" Name=\"X\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Y\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_RIGHT_HAND[1].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa9_Y0\" MaxVal=\"\" MinVal=\"\" Name=\"Y\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Z\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_RIGHT_HAND[2].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa9_Z0\" MaxVal=\"\" MinVal=\"\" Name=\"Z\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /></Generic><Generic Category=\"MoCapMarker\" ID=\"ROOT0_MoCa0_MoCa10\" Name=\"LeftKnee\" _Extra=\"\" Expanded=\"0\"><Signal BytesPerSample=\"\" Category=\"X\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_LEFT_KNEE[0].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa10_X0\" MaxVal=\"\" MinVal=\"\" Name=\"X\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Y\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_LEFT_KNEE[1].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa10_Y0\" MaxVal=\"\" MinVal=\"\" Name=\"Y\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Z\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_LEFT_KNEE[2].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa10_Z0\" MaxVal=\"\" MinVal=\"\" Name=\"Z\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /></Generic><Generic Category=\"MoCapMarker\" ID=\"ROOT0_MoCa0_MoCa11\" Name=\"LeftHip\" _Extra=\"\" Expanded=\"0\"><Signal BytesPerSample=\"\" Category=\"X\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_LEFT_HIP[0].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa11_X0\" MaxVal=\"\" MinVal=\"\" Name=\"X\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Y\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_LEFT_HIP[1].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa11_Y0\" MaxVal=\"\" MinVal=\"\" Name=\"Y\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Z\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_LEFT_HIP[2].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa11_Z0\" MaxVal=\"\" MinVal=\"\" Name=\"Z\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /></Generic><Generic Category=\"MoCapMarker\" ID=\"ROOT0_MoCa0_MoCa12\" Name=\"LeftFoot\" _Extra=\"\" Expanded=\"0\"><Signal BytesPerSample=\"\" Category=\"X\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_LEFT_FOOT[0].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa12_X0\" MaxVal=\"\" MinVal=\"\" Name=\"X\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Y\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_LEFT_FOOT[1].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa12_Y0\" MaxVal=\"\" MinVal=\"\" Name=\"Y\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Z\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_LEFT_FOOT[2].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa12_Z0\" MaxVal=\"\" MinVal=\"\" Name=\"Z\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /></Generic><Generic Category=\"MoCapMarker\" ID=\"ROOT0_MoCa0_MoCa13\" Name=\"RightKnee\" _Extra=\"\" Expanded=\"0\"><Signal BytesPerSample=\"\" Category=\"X\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_RIGHT_KNEE[0].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa13_X0\" MaxVal=\"\" MinVal=\"\" Name=\"X\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Y\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_RIGHT_KNEE[1].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa13_Y0\" MaxVal=\"\" MinVal=\"\" Name=\"Y\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Z\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_RIGHT_KNEE[2].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa13_Z0\" MaxVal=\"\" MinVal=\"\" Name=\"Z\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /></Generic><Generic Category=\"MoCapMarker\" ID=\"ROOT0_MoCa0_MoCa14\" Name=\"RightHip\" _Extra=\"\" Expanded=\"0\"><Signal BytesPerSample=\"\" Category=\"X\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_RIGHT_HIP[0].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa14_X0\" MaxVal=\"\" MinVal=\"\" Name=\"X\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Y\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_RIGHT_HIP[1].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa14_Y0\" MaxVal=\"\" MinVal=\"\" Name=\"Y\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Z\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_RIGHT_HIP[2].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa14_Z0\" MaxVal=\"\" MinVal=\"\" Name=\"Z\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /></Generic><Generic Category=\"MoCapMarker\" ID=\"ROOT0_MoCa0_MoCa15\" Name=\"RightFoot\" _Extra=\"\" Expanded=\"0\"><Signal BytesPerSample=\"\" Category=\"X\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_RIGHT_FOOT[0].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa15_X0\" MaxVal=\"\" MinVal=\"\" Name=\"X\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Y\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_RIGHT_FOOT[1].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa15_Y0\" MaxVal=\"\" MinVal=\"\" Name=\"Y\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /><Signal BytesPerSample=\"\" Category=\"Z\" DefaultPath=\"0\" EstimatedSampleRate=\"0.0\" Expanded=\"1\" FileType=\"CSV\" Filename=\"JOINT_RIGHT_FOOT[2].csv\" FrameSize=\"\" ID=\"ROOT0_MoCa0_MoCa15_Z0\" MaxVal=\"\" MinVal=\"\" Name=\"Z\" NumChannels=\"\" NumSamples=\"\" ResampledFlag=\"-1\" SpecSampleRate=\"0.0\" _Extra=\"canvas=-1,color=0,selected=1\" SampleRate=\"30\" /></Generic></Generic>";
    
    if (recordAudio) {
        xml << "<Generic ID=\"ROOT0_Audi0\" Category=\"AudioGroup\" Name=\"Name of Audio Group\" _Extra=\"\" Expanded=\"1\"><Audio ID=\"ROOT0_Audi0_Ambi0\" Expanded=\"1\" Category=\"Ambient\" Name=\"Audio\" FileType=\"WAV\" DefaultPath=\"0\" Filename=\"audio.wav\" _Extra=\"canvas=-1,color=0,selected=1\" BytesPerSample=\"2\" FrameSize=\"1\" NumChannels=\"2\" NumSamples=\"303440\" SampleRate=\"44100\" SpecSampleRate=\"0.0\" EstimatedSampleRate=\"0.0\" ResampledFlag=\"-1\" /></Generic>";
    }
    if (audio) {
        xml << "<Generic ID=\"ROOT0_Audi0\" Category=\"AudioGroup\" Name=\"Name of Audio Group\" _Extra=\"\" Expanded=\"1\"><Audio ID=\"ROOT0_Audi0_Ambi0\" Expanded=\"1\" Category=\"Ambient\" Name=\"Audio\" FileType=\"WAV\" DefaultPath=\"0\" Filename=\"" << ((ofxUIRadio*) gui->getWidget("AUDIOS"))->getActiveName() << "\" _Extra=\"canvas=-1,color=0,selected=1\" BytesPerSample=\"2\" FrameSize=\"1\" NumChannels=\"2\" NumSamples=\"303440\" SampleRate=\"44100\" SpecSampleRate=\"0.0\" EstimatedSampleRate=\"0.0\" ResampledFlag=\"-1\" /></Generic>";
    }
    if (recordVideo) {
        xml << "<Generic ID=\"ROOT0_Vide0\" Category=\"VideoGroup\" Name=\"Name of Video Group\" _Extra=\"\" Expanded=\"1\"><Video ID=\"ROOT0_Vide0_LQ0\" Expanded=\"1\" Category=\"LQ\" Name=\"RGB\" FileType=\"MOV\" DefaultPath=\"0\" Filename=\"rgb.mov\" _Extra=\"canvas=-1,color=0,selected=1\" /><Video ID=\"ROOT0_Vide0_HQ0\" Expanded=\"1\" Category=\"HQ\" Name=\"RGBD\" FileType=\"MOV\" DefaultPath=\"0\" Filename=\"rgbd.mov\" _Extra=\"canvas=-1,color=0,selected=1\" /></Generic>";
    }
    
    xml << "</ROOT>";
    xml.close();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){
    
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){
    
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){
    
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
    
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){
    
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){
    
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){
    
}
