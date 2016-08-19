#include "ros/ros.h"
#include "std_msgs/String.h"
#include <sensor_msgs/PointCloud2.h>
#include <string.h>

#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>

#include "eigen_conversions/eigen_msg.h"
#include "tf_conversions/tf_eigen.h"

#include "metaroom_xml_parser/simple_xml_parser.h"
#include "metaroom_xml_parser/simple_summary_parser.h"

#include <tf_conversions/tf_eigen.h>

#include "ros/ros.h"
#include <metaroom_xml_parser/load_utilities.h>
#include <pcl_ros/point_cloud.h>
#include <cv_bridge/cv_bridge.h>

#include "metaroom_xml_parser/load_utilities.h"
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl_ros/transforms.h>

#include <tf_conversions/tf_eigen.h>

#include "quasimodo_msgs/model.h"
#include "quasimodo_msgs/rgbd_frame.h"
#include "quasimodo_msgs/model_from_frame.h"
#include "quasimodo_msgs/index_frame.h"
#include "quasimodo_msgs/fuse_models.h"
#include "quasimodo_msgs/get_model.h"
#include "quasimodo_msgs/segment_model.h"
#include "quasimodo_msgs/metaroom_pair.h"

#include "ros/ros.h"
#include <quasimodo_msgs/query_cloud.h>
#include <quasimodo_msgs/visualize_query.h>
#include <metaroom_xml_parser/load_utilities.h>
#include <pcl_ros/point_cloud.h>
#include <cv_bridge/cv_bridge.h>

#include "metaroom_xml_parser/load_utilities.h"
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl_ros/transforms.h>

#include "quasimodo_msgs/model.h"
#include "quasimodo_msgs/rgbd_frame.h"
#include "quasimodo_msgs/model_from_frame.h"
#include "quasimodo_msgs/index_frame.h"
#include "quasimodo_msgs/fuse_models.h"
#include "quasimodo_msgs/get_model.h"

#include "metaroom_xml_parser/simple_xml_parser.h"
#include "metaroom_xml_parser/load_utilities.h"

#include <image_geometry/pinhole_camera_model.h>
#include <sensor_msgs/CameraInfo.h>

#include "modelupdater/ModelUpdater.h"
#include "core/RGBDFrame.h"
#include "Util/Util.h"

ros::ServiceClient segmentation_client;
using namespace std;
using namespace semantic_map_load_utilties;

typedef pcl::PointXYZRGB PointType;
typedef pcl::PointCloud<PointType> Cloud;
typedef typename Cloud::Ptr CloudPtr;
typedef pcl::search::KdTree<PointType> Tree;
typedef semantic_map_load_utilties::DynamicObjectData<PointType> ObjectData;

using namespace std;
using namespace semantic_map_load_utilties;

/*
bool segment_metaroom(quasimodo_msgs::metaroom_pair::Request  & req, quasimodo_msgs::metaroom_pair::Response & res){
	printf("segment_metaroom\n");

	printf("background: %s\n",req.background.c_str());
	printf("foreground: %s\n",req.foreground.c_str());

	reglib::Model * bg_model = quasimodo_brain::load_metaroom_model(req.background);
	reglib::Model * fg_model = quasimodo_brain::load_metaroom_model(req.foreground);

	quasimodo_msgs::segment_model sm;
	sm.request.backgroundmodel = quasimodo_brain::getModelMSG(bg_model);
	sm.request.models.push_back(quasimodo_brain::getModelMSG(fg_model));

	bool status;

	if (segmentation_client.call(sm)){
		if(sm.response.dynamicmasks.size() > 0){
			res.dynamicmasks	= sm.response.dynamicmasks.front().images;
			res.movingmasks		= sm.response.movingmasks.front().images;
		}
		status = true;
	}else{
		ROS_ERROR("Failed to call service segment_model");
		status = false;
	}

	bg_model->fullDelete();
	delete bg_model;
	fg_model->fullDelete();
	delete fg_model;
	return status;
}
*/

std::string overall_folder = "~/.semanticMap/";
boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer;
int visualization_lvl = 0;
std::string outtopic = "/some/topic";
ros::Publisher out_pub;

void processMetaroom(std::string path){
	printf("processing: %s\n",path.c_str());

	int slash_pos = path.find_last_of("/");
	std::string sweep_folder = path.substr(0, slash_pos) + "/";

	std::vector<cv::Mat> viewrgbs;
	std::vector<cv::Mat> viewdepths;
	std::vector<tf::StampedTransform > viewtfs;

	QStringList objectFiles = QDir(sweep_folder.c_str()).entryList(QStringList("*object*.xml"));
	for (auto objectFile : objectFiles){
		auto object = loadDynamicObjectFromSingleSweep<PointType>(sweep_folder+objectFile.toStdString(),false);
		for (unsigned int i=0; i<object.vAdditionalViews.size(); i++){

			CloudPtr cloud = object.vAdditionalViews[i];


			cv::Mat rgb;
			rgb.create(cloud->height,cloud->width,CV_8UC3);
			unsigned char * rgbdata = (unsigned char *)rgb.data;

			cv::Mat depth;
			depth.create(cloud->height,cloud->width,CV_16UC1);
			unsigned short * depthdata = (unsigned short *)depth.data;

			unsigned int nr_data = cloud->height * cloud->width;
			for(unsigned int j = 0; j < nr_data; j++){
				PointType p = cloud->points[j];
				rgbdata[3*j+0]	= p.b;
				rgbdata[3*j+1]	= p.g;
				rgbdata[3*j+2]	= p.r;
				depthdata[j]	= short(5000.0 * p.z);
			}

			viewrgbs.push_back(rgb);
			viewdepths.push_back(depth);
			viewtfs.push_back(object.vAdditionalViewsTransforms[i]);
			//viewcams.push_back(roomData.vIntermediateRoomCloudCamParams.front());

			cv::namedWindow("rgbimage",     cv::WINDOW_AUTOSIZE);
			cv::imshow(		"rgbimage",     rgb);
			cv::namedWindow("depthimage",	cv::WINDOW_AUTOSIZE);
			cv::imshow(		"depthimage",	depth);
			cv::waitKey(30);

		}
	}

	reglib::Model * sweep = quasimodo_brain::load_metaroom_model(path);


	std::vector<reglib::RGBDFrame *> frames;
	std::vector<reglib::ModelMask *> masks;
	std::vector<Eigen::Matrix4d> unrefined;

	std::vector<Eigen::Matrix4d> both_unrefined;
	both_unrefined.push_back(Eigen::Matrix4d::Identity());
	std::vector<double> times;
	for(unsigned int i = 0; i < viewrgbs.size(); i++){
		printf("additional view: %i\n",i);
		geometry_msgs::TransformStamped msg;
		tf::transformStampedTFToMsg(viewtfs[i], msg);
		long sec = msg.header.stamp.sec;
		long nsec = msg.header.stamp.nsec;
		double time = double(sec)+1e-9*double(nsec);

		Eigen::Matrix4d m = quasimodo_brain::getMat(viewtfs[i]);

		cout << m << endl << endl;

		unrefined.push_back(m);
		times.push_back(time);

		cv::Mat fullmask;
		fullmask.create(480,640,CV_8UC1);
		unsigned char * maskdata = (unsigned char *)fullmask.data;
		for(int j = 0; j < 480*640; j++){maskdata[j] = 255;}
		masks.push_back(new reglib::ModelMask(fullmask));

		reglib::Camera * cam		= sweep->frames.front()->camera->clone();
		reglib::RGBDFrame * frame	= new reglib::RGBDFrame(cam,viewrgbs[i],viewdepths[i],time, m);//a.matrix());
		frames.push_back(frame);

		//both_unrefined.push_back(sweep->frames.front()->pose.inverse()*a.matrix());
		both_unrefined.push_back(sweep->frames.front()->pose.inverse()*m);
	}

	reglib::RegistrationRandom *	reg	= new reglib::RegistrationRandom();
	reglib::ModelUpdaterBasicFuse * mu	= new reglib::ModelUpdaterBasicFuse( sweep, reg);
	mu->occlusion_penalty               = 15;
	mu->massreg_timeout                 = 60*4;
	mu->viewer							= viewer;


	sweep->points = mu->getSuperPoints(sweep->relativeposes,sweep->frames,sweep->modelmasks,1,false);



	//Not needed if metaroom well calibrated
	reglib::MassRegistrationPPR2 * bgmassreg = new reglib::MassRegistrationPPR2(0.01);
	bgmassreg->timeout = 20;
	bgmassreg->viewer = viewer;
	bgmassreg->use_surface = true;
	bgmassreg->use_depthedge = false;
	bgmassreg->visualizationLvl = visualization_lvl;
	bgmassreg->maskstep = 15;
	bgmassreg->nomaskstep = 15;
	bgmassreg->nomask = true;
	bgmassreg->stopval = 0.0005;
	bgmassreg->addModel(sweep);
	bgmassreg->setData(frames,masks);
	reglib::MassFusionResults bgmfr = bgmassreg->getTransforms(both_unrefined);


	SimpleXMLParser<pcl::PointXYZRGB> parser;
	SimpleXMLParser<pcl::PointXYZRGB>::RoomData current_roomData  = parser.loadRoomFromXML(path);
	std::string current_waypointid = current_roomData.roomWaypointId;

	if(overall_folder.back() == '/'){overall_folder.pop_back();}


	int prevind = -1;
	std::vector<std::string> sweep_xmls = semantic_map_load_utilties::getSweepXmls<pcl::PointXYZRGB>(overall_folder);
	for (int i = 0; i < sweep_xmls.size(); i++){

		SimpleXMLParser<pcl::PointXYZRGB>::RoomData other_roomData  = parser.loadRoomFromXML(sweep_xmls[i],std::vector<std::string>(),false,false);
		std::string other_waypointid = other_roomData.roomWaypointId;


		if(sweep_xmls[i].compare(path) == 0){
			break;
		}
		if(other_waypointid.compare(current_waypointid) == 0){
			prevind = i;
		}
	}

	if(prevind != -1){
		std::string prev = sweep_xmls[prevind];
		printf("prev: %s\n",prev.c_str());

		reglib::Model * bg = quasimodo_brain::load_metaroom_model(prev);
		bg->points = mu->getSuperPoints(bg->relativeposes,bg->frames,bg->modelmasks,1,false);

		std::vector< reglib::Model * > models;
		models.push_back(sweep);


		std::vector< std::vector< cv::Mat > > internal;
		std::vector< std::vector< cv::Mat > > external;
		std::vector< std::vector< cv::Mat > > dynamic;

		quasimodo_brain::segment(bg,models,internal,external,dynamic);



		for(unsigned int i = 0; visualization_lvl > 0 && i < models.size(); i++){
			std::vector<cv::Mat> internal_masks = internal[i];
			std::vector<cv::Mat> external_masks = external[i];
			std::vector<cv::Mat> dynamic_masks	= dynamic[i];
			reglib::Model * model = models[i];


			std::vector<Eigen::Matrix4d> mod_po;
			std::vector<reglib::RGBDFrame*> mod_fr;
			std::vector<reglib::ModelMask*> mod_mm;
			model->getData(mod_po, mod_fr, mod_mm);

			pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr cloud (new pcl::PointCloud<pcl::PointXYZRGBNormal>);

//			for(unsigned int j = 0; j < model->frames.size(); j++){
//				reglib::RGBDFrame * frame = model->frames[j];
//				Eigen::Matrix4d p = model->relativeposes[j];
			for(unsigned int j = 0; j < mod_fr.size(); j++){
				reglib::RGBDFrame * frame = mod_fr[j];
				Eigen::Matrix4d p = mod_po[j];
				unsigned char  * rgbdata		= (unsigned char	*)(frame->rgb.data);
				unsigned short * depthdata		= (unsigned short	*)(frame->depth.data);
				float		   * normalsdata	= (float			*)(frame->normals.data);

				reglib::Camera * camera = frame->camera;

				unsigned char * internalmaskdata = (unsigned char *)(internal_masks[j].data);
				unsigned char * externalmaskdata = (unsigned char *)(external_masks[j].data);
				unsigned char * dynamicmaskdata = (unsigned char *)(dynamic_masks[j].data);


				float m00 = p(0,0); float m01 = p(0,1); float m02 = p(0,2); float m03 = p(0,3);
				float m10 = p(1,0); float m11 = p(1,1); float m12 = p(1,2); float m13 = p(1,3);
				float m20 = p(2,0); float m21 = p(2,1); float m22 = p(2,2); float m23 = p(2,3);

				const float idepth			= camera->idepth_scale;
				const float cx				= camera->cx;
				const float cy				= camera->cy;
				const float ifx				= 1.0/camera->fx;
				const float ify				= 1.0/camera->fy;
				const unsigned int width	= camera->width;
				const unsigned int height	= camera->height;


				for(unsigned int w = 0; w < width;w++){
					for(unsigned int h = 0; h < height;h++){
						int ind = h*width+w;
						float z = idepth*float(depthdata[ind]);
						if(z > 0){
							float x = (float(w) - cx) * z * ifx;
							float y = (float(h) - cy) * z * ify;

							pcl::PointXYZRGBNormal point;
							point.x = m00*x + m01*y + m02*z + m03;
							point.y = m10*x + m11*y + m12*z + m13;
							point.z = m20*x + m21*y + m22*z + m23;

							point.b = rgbdata[3*ind+0];
							point.g = rgbdata[3*ind+1];
							point.r = rgbdata[3*ind+2];

							if(dynamicmaskdata[ind] != 0){
								point.b = 0;
								point.g = 255;
								point.r = 0;
							}else if(internalmaskdata[ind] == 0){
								point.b = 0;
								point.g = 0;
								point.r = 255;
							}

							cloud->points.push_back(point);
						}
					}
				}
			}

			viewer->removeAllPointClouds();
			viewer->addPointCloud<pcl::PointXYZRGBNormal> (cloud, pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGBNormal>(cloud), "scloud");
			viewer->spin();
			//while(cv::waitKey(50)!='q'){viewer->spinOnce();}
		}

	}



	delete bgmassreg;
	delete reg;
	delete mu;

//    xmlWriter->writeStartElement("Transform");
//    xmlWriter->writeStartElement("Translation");
//    xmlWriter->writeStartElement("x");
//    xmlWriter->writeCharacters(QString::number(msg.transform.translation.x));
//    xmlWriter->writeEndElement();
//    xmlWriter->writeStartElement("y");
//    xmlWriter->writeCharacters(QString::number(msg.transform.translation.y));
//    xmlWriter->writeEndElement();
//    xmlWriter->writeStartElement("z");
//    xmlWriter->writeCharacters(QString::number(msg.transform.translation.z));
//    xmlWriter->writeEndElement();
//    xmlWriter->writeEndElement(); // Translation
//    xmlWriter->writeStartElement("Rotation");
//    xmlWriter->writeStartElement("w");
//    xmlWriter->writeCharacters(QString::number(msg.transform.rotation.w));
//    xmlWriter->writeEndElement();
//    xmlWriter->writeStartElement("x");
//    xmlWriter->writeCharacters(QString::number(msg.transform.rotation.x));
//    xmlWriter->writeEndElement();
//    xmlWriter->writeStartElement("y");
//    xmlWriter->writeCharacters(QString::number(msg.transform.rotation.y));
//    xmlWriter->writeEndElement();
//    xmlWriter->writeStartElement("z");
//    xmlWriter->writeCharacters(QString::number(msg.transform.rotation.z));

	std_msgs::String msg;
	msg.data = path;
	out_pub.publish(msg);
	ros::spinOnce();
}

void chatterCallback(const std_msgs::String::ConstPtr& msg){
	processMetaroom(msg->data);
}

void segmentWithAdditionalViewsXml(std::string sweep_xml,std::string prev){
	printf("segmentWithAdditionalViewsXml\n");
//	std::string prev = getPrevXML(sweep_xml);
}

int main(int argc, char** argv){
	ros::init(argc, argv, "metaroom_additional_view_processing");
	ros::NodeHandle n;



	int inputstate = 0;
	for(int i = 1; i < argc;i++){
		printf("input: %s\n",argv[i]);
		if(std::string(argv[i]).compare("-file") == 0){inputstate = 2;}
		else if(std::string(argv[i]).compare("-intopic") == 0){inputstate = 0;}
		else if(std::string(argv[i]).compare("-outtopic") == 0){inputstate = 1;}
		else if(std::string(argv[i]).compare("-folder") == 0){inputstate = 4;}
		else if(std::string(argv[i]).compare("-v") == 0){
			viewer = boost::shared_ptr<pcl::visualization::PCLVisualizer>(new pcl::visualization::PCLVisualizer ("3D Viewer"));
			viewer->setBackgroundColor (0.5, 0, 0.5);
			viewer->addCoordinateSystem (1.0);
			viewer->initCameraParameters ();
			visualization_lvl = 1;
			inputstate = 3;
		}else if(inputstate == 0){
			out_pub = n.advertise<std_msgs::String>(outtopic, 1000);
			ros::Subscriber sub = n.subscribe(std::string(argv[i]), 1000, chatterCallback);
			ros::spin();
		}else if(inputstate == 1){
			outtopic = std::string(argv[i]);
		}else if(inputstate == 3){
			visualization_lvl = atoi(argv[i]);
		}else if(inputstate == 4){
			overall_folder = std::string(argv[i]);
		}else{
			out_pub = n.advertise<std_msgs::String>(outtopic, 1000);
			processMetaroom(std::string(argv[i]));
		}
	}
	/*
    reglib::RegistrationRandom *	reg	= new reglib::RegistrationRandom();



    for(int ar = 1; ar < argc; ar++){
        string overall_folder = std::string(argv[ar]);
        vector<string> sweep_xmls = semantic_map_load_utilties::getSweepXmls<PointType>(overall_folder);
        printf("sweep_xmls\n");
        for (auto sweep_xml : sweep_xmls) {
            printf("sweep_xml: %s\n",sweep_xml.c_str());
            load2(sweep_xml);
        }
    }




	ros::init(argc, argv, "test_segment");
	ros::NodeHandle n;
	ros::ServiceClient segmentation_client = n.serviceClient<quasimodo_msgs::segment_model>("segment_model");

	for(unsigned int i = 1; i < models.size(); i++){
		quasimodo_msgs::segment_model sm;
		sm.request.models.push_back(quasimodo_brain::getModelMSG(models[i]));

		if(i > 0){
			sm.request.backgroundmodel = quasimodo_brain::getModelMSG(models[i-1]);
		}

		if (segmentation_client.call(sm)){//Build model from frame
			//int model_id = mff.response.model_id;
			printf("segmented: %i\n",i);
		}else{ROS_ERROR("Failed to call service segment_model");}
	}
	//ros::spin();

    delete reg;
    for(size_t j = 0; j < models.size(); j++){
        models[j]->fullDelete();
        delete models[j];
    }
	printf("done\n");
	*/
	return 0;
}