#include "k2class.h"

//std::vector<pcl::PointCloud<pcl::PointXYZ>::ConstPtr> clouds;

k2class::k2class(QObject *parent) : QObject(parent),
    viewer (new pcl::visualization::PCLVisualizer ("Viewer 2")),
    clouds (0),
    p2m (nullptr),
    final_cloud (new pcl::PointCloud<pcl::PointXYZ>)
{
}

void k2class::init ()
{

    boost::mutex mt;
    boost::shared_ptr<pcl::Grabber> grabber = boost::make_shared<pcl::Kinect2Grabber>();
    pcl::PointCloud<pcl::PointXYZ>::ConstPtr cloud;

    viewer->setCameraPosition( 0.0, 0.0, -2.5, 0.0, 0.0, 0.0 );
    boost::function<void( const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& )> func_cb =
            [&cloud, &mt, this]( const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& ptr)
    {
        boost::mutex::scoped_lock lock(mt);
        cloud = ptr->makeShared();
    };


    boost::signals2::connection conn = grabber->registerCallback(func_cb);

    grabber->start();

    while (!viewer->wasStopped())
    {
        viewer->spinOnce();

        boost::mutex::scoped_try_lock lock_mutex2 (mt);
        if( lock_mutex2.owns_lock() && cloud){

            int time = 0;

            pcl::PointCloud<pcl::PointXYZ>::Ptr filter_cloud (new pcl::PointCloud<pcl::PointXYZ>);
            pcl::PassThrough<pcl::PointXYZ> filter;
            filter.setInputCloud(cloud);
            filter.setFilterFieldName("z");
            filter.setFilterLimits(0.0, 1.0);
            filter.filter(*filter_cloud);

            std::stringstream ss;
            ss << time << ".ply";
            pcl::io::savePCDFile(ss.str(), *filter_cloud, true);

            clouds.push_back(filter_cloud);
            time++;

            if(!viewer->updatePointCloud(cloud, "cloud")){
                viewer->addPointCloud(cloud, "cloud");
            }

            //time++;
        }
    }

    grabber->stop ();
}

void k2class::registration(std::vector<pcl::PointCloud<pcl::PointXYZ>::ConstPtr> clouds)
{

    //pcl::PointCloud<pcl::PointXYZ>::Ptr final_cloud (new pcl::PointCloud<pcl::PointXYZ>);
    pcl::PointCloud<pcl::PointXYZ>::ConstPtr c1, c2;
    Eigen::Matrix4f transf_m = Eigen::Matrix4f::Identity (), transf2p2;

    for (int i = 1; i < clouds.size(); i++)
    {
        c1 = clouds.at(i-1);
        c2 = clouds.at(i);

        pcl::PointCloud<pcl::PointXYZ>::Ptr tmp (new pcl::PointCloud<pcl::PointXYZ>);

        p2m.estimate_align(c1, c2, tmp, transf2p2, true);

        pcl::transformPointCloud (*tmp, *final_cloud, transf_m);
        transf_m = transf_m * transf2p2;
    }

    pcl::io::savePCDFile("point.pcd", *final_cloud, true);
    pcl::io::savePLYFile("point.ply", *final_cloud, true);

    //    pcl::PCLPointCloud2 cloud;
    //    pcl::io::loadPCDFile(ss.str(), cloud);

    //    p2m.point2mesh(cloud);

    //    std::stringstream ss1;
    //    ss1 << "teste1.ply";
    //    pcl::PLYWriter writer;
    //    writer.write (ss1.str(), cloud , Eigen::Vector4f::Zero (),
    //                  Eigen::Quaternionf::Identity (), true, true);

    //    pcl::PolygonMesh mesh;
    //    pcl::io::loadPLYFile(ss1.str(), mesh);

    std::cout << "FINISHED" << std::endl;
}
